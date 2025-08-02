/**
 * @file ConfigWatcher.cpp
 * @brief Real-time configuration file monitoring system implementation
 * 
 * Uses Linux inotify for file watching, ensuring automatic configuration reload
 * when jobs.json is modified. Thread-safe with mutex protection for concurrent
 * access to job data structures.
 */

#include "ConfigWatcher.h"
#include "JobConfig.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <limits.h>

/**
 * Constructor - Initializes watcher and loads initial configuration
 * @param jobsPath Path to the JSON configuration file
 * @param loggerRef Reference to centralized logging system
 */
ConfigWatcher::ConfigWatcher(const std::string& jobsPath, Logger& loggerRef) 
    : configPath(jobsPath), logger(loggerRef), inotifyFd(-1), watchDescriptor(-1) {
    
    // Bootstrap initial configuration loading
    currentJobs = loadJobsFromFile();
    if (!currentJobs) {
        // Fallback: create empty container to avoid null pointers
        currentJobs = std::make_shared<std::vector<CronJob>>();
        logger.error("ConfigWatcher: Failed to load initial configuration from " + configPath);
    } else {
        logger.info("ConfigWatcher: Loaded " + std::to_string(currentJobs->size()) + " jobs from " + configPath);
    }
}

/**
 * Destructor - Automatic cleanup of inotify resources and threads
 */
ConfigWatcher::~ConfigWatcher() {
    stopWatching();
}

/**
 * Starts file monitoring system with dedicated thread
 * @return true if watching started successfully
 * @note Uses atomics for thread-safe state control
 */
bool ConfigWatcher::startWatching() {
    if (isWatching.load()) {
        logger.warning("ConfigWatcher: Already watching configuration file");
        return true;
    }
    
    // Initialize Linux inotify subsystem
    if (!initializeInotify()) {
        logger.error("ConfigWatcher: Failed to initialize inotify");
        return false;
    }
    
    // Set atomic flags for thread synchronization
    shouldStop.store(false);
    isWatching.store(true);
    
    try {
        // Spawn dedicated watcher thread
        watcherThread = std::thread(&ConfigWatcher::watcherLoop, this);
        logger.info("ConfigWatcher: Started watching " + configPath);
        return true;
    } catch (const std::exception& e) {
        logger.error("ConfigWatcher: Failed to start watcher thread: " + std::string(e.what()));
        cleanupInotify();
        isWatching.store(false);
        return false;
    }
}

/**
 * Stops file monitoring and performs cleanup
 * Thread-safe shutdown with proper resource deallocation
 */
void ConfigWatcher::stopWatching() {
    if (!isWatching.load()) {
        return;
    }
    
    // Signal thread to stop and wait for graceful shutdown
    shouldStop.store(true);
    isWatching.store(false);
    
    if (watcherThread.joinable()) {
        watcherThread.join();
    }
    
    cleanupInotify();
    logger.info("ConfigWatcher: Stopped watching configuration file");
}

/**
 * Thread-safe accessor for current job configuration
 * @return Shared pointer to current job vector (thread-safe)
 */
std::shared_ptr<std::vector<CronJob>> ConfigWatcher::getJobs() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return currentJobs;
}

/**
 * Validates current configuration state
 * @return true if configuration is loaded and contains jobs
 */
bool ConfigWatcher::isConfigValid() const {
    return currentJobs && !currentJobs->empty();
}

/**
 * Forces immediate configuration reload (bypasses file watching)
 * @return true if reload successful
 */
bool ConfigWatcher::forceReload() {
    return validateAndLoadConfig();
}

/**
 * Initializes Linux inotify file watching system
 * @return true if inotify setup successful
 * @note Sets up non-blocking mode with appropriate event masks
 */
bool ConfigWatcher::initializeInotify() {
    // Create inotify instance with non-blocking flag
    inotifyFd = inotify_init1(IN_NONBLOCK);
    if (inotifyFd == -1) {
        logger.error("ConfigWatcher: Failed to initialize inotify");
        return false;
    }
    
    // Monitor file modifications, writes, and moves (editor save patterns)
    watchDescriptor = inotify_add_watch(inotifyFd, configPath.c_str(), 
                                       IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO);
    
    if (watchDescriptor == -1) {
        logger.error("ConfigWatcher: Failed to add watch for " + configPath);
        close(inotifyFd);
        inotifyFd = -1;
        return false;
    }
    
    return true;
}

/**
 * Cleanup inotify resources and file descriptors
 * Safe to call multiple times
 */
void ConfigWatcher::cleanupInotify() {
    if (watchDescriptor != -1) {
        inotify_rm_watch(inotifyFd, watchDescriptor);
        watchDescriptor = -1;
    }
    
    if (inotifyFd != -1) {
        close(inotifyFd);
        inotifyFd = -1;
    }
}

/**
 * Main watcher thread loop - monitors inotify events
 * Uses select() with timeout for responsive shutdown
 * Handles file change events and triggers configuration reload
 */
void ConfigWatcher::watcherLoop() {
    logger.info("ConfigWatcher: Watcher thread started");
    
    // Fixed buffer size for inotify event reading (standard size)
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    
    while (!shouldStop.load()) {
        // Setup file descriptor set for select()
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(inotifyFd, &readfds);
        
        // Timeout for responsive thread shutdown
        struct timeval timeout;
        timeout.tv_sec = 1;  // Check every second
        timeout.tv_usec = 0;
        
        int result = select(inotifyFd + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (result == -1) {
            if (errno != EINTR) {
                logger.error("ConfigWatcher: select() failed: " + std::string(strerror(errno)));
                break;
            }
            continue;
        }
        
        if (result == 0) {
            // Timeout - continue monitoring loop
            continue;
        }
        
        // Process inotify events
        if (FD_ISSET(inotifyFd, &readfds)) {
            ssize_t bytesRead = read(inotifyFd, buffer, bufferSize);
            
            if (bytesRead == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    logger.error("ConfigWatcher: read() failed: " + std::string(strerror(errno)));
                    break;
                }
                continue;
            }
            
            if (bytesRead > 0) {
                logger.info("ConfigWatcher: Configuration file changed, reloading...");
                
                // Brief delay to ensure file write completion (editor save patterns)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                if (validateAndLoadConfig()) {
                    logger.success("ConfigWatcher: Configuration reloaded successfully");
                } else {
                    logger.error("ConfigWatcher: Failed to reload configuration - keeping old version");
                }
            }
        }
    }
    
    logger.info("ConfigWatcher: Watcher thread stopped");
}

/**
 * Validates and atomically loads new configuration
 * @return true if validation and loading successful
 * @note Performs validation before replacing current config to ensure stability
 */
bool ConfigWatcher::validateAndLoadConfig() {
    try {
        // Pre-validation to catch syntax errors before loading
        std::string errorMsg;
        if (!JobConfig::validateJobsFile(configPath, errorMsg)) {
            logger.error("ConfigWatcher: Configuration validation failed: " + errorMsg);
            return false;
        }
        
        // Attempt to load new configuration
        auto newJobs = loadJobsFromFile();
        
        if (!newJobs) {
            logger.error("ConfigWatcher: Failed to load new configuration");
            return false;
        }
        
        // Additional semantic validation for loaded jobs
        for (const auto& job : *newJobs) {
            if (job.command.empty()) {
                logger.error("ConfigWatcher: Invalid job found - empty command");
                return false;
            }
            
            if (job.description.empty()) {
                logger.warning("ConfigWatcher: Job with empty description: " + job.command);
            }
        }
        
        // Atomic replacement of current configuration (thread-safe)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            currentJobs = newJobs;
        }
        
        logger.info("ConfigWatcher: Successfully reloaded " + std::to_string(newJobs->size()) + " jobs");
        return true;
        
    } catch (const std::exception& e) {
        logger.error("ConfigWatcher: Exception during config reload: " + std::string(e.what()));
        return false;
    }
}

/**
 * Loads jobs from configuration file using JobConfig infrastructure
 * @return Shared pointer to job vector, nullptr on failure
 * @note Performs file existence check before attempting load
 */
std::shared_ptr<std::vector<CronJob>> ConfigWatcher::loadJobsFromFile() {
    try {
        // Verify file accessibility before processing
        std::ifstream testFile(configPath);
        if (!testFile.is_open()) {
            logger.error("ConfigWatcher: Configuration file does not exist or is not readable: " + configPath);
            return nullptr;
        }
        testFile.close();
        
        // Delegate to existing JobConfig parsing infrastructure
        std::vector<CronJob> jobs = JobConfig::loadJobs(configPath);
        
        // Handle empty configuration case
        if (jobs.empty()) {
            logger.warning("ConfigWatcher: No jobs loaded from configuration file");
            return std::make_shared<std::vector<CronJob>>();
        }
        
        return std::make_shared<std::vector<CronJob>>(std::move(jobs));
        
    } catch (const std::exception& e) {
        logger.error("ConfigWatcher: Exception loading jobs from file: " + std::string(e.what()));
        return nullptr;
    }
}