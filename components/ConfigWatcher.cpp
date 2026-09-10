/**
 * @file ConfigWatcher.cpp
 * @brief Real-time configuration file monitoring using Linux inotify.
 */

#include "ConfigWatcher.h"
#include "JobConfig.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <filesystem>

ConfigWatcher::ConfigWatcher(const std::string& jobsPath, Logger& loggerRef)
    : configPath(jobsPath), logger(loggerRef), inotifyFd(-1), watchDescriptor(-1) {

    // Split the path once here: inotify will watch the directory, not the file.
    std::filesystem::path p(jobsPath);
    watchDir = p.parent_path().empty() ? "." : p.parent_path().string();
    watchedFilename = p.filename().string();

    currentJobs = loadJobsFromFile();
    if (!currentJobs) {
        currentJobs = std::make_shared<std::vector<CronJob>>();
        logger.error("ConfigWatcher: Failed to load initial configuration from " + configPath);
    } else {
        logger.info("ConfigWatcher: Loaded " + std::to_string(currentJobs->size()) + " jobs from " + configPath);
    }
}

ConfigWatcher::~ConfigWatcher() {
    stopWatching();
}

bool ConfigWatcher::startWatching() {
    if (isWatching.load()) {
        logger.warning("ConfigWatcher: Already watching configuration file");
        return true;
    }

    if (!initializeInotify()) {
        logger.error("ConfigWatcher: Failed to initialize inotify");
        return false;
    }

    shouldStop.store(false);
    isWatching.store(true);

    try {
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

void ConfigWatcher::stopWatching() {
    if (!isWatching.load()) {
        return;
    }

    shouldStop.store(true);
    isWatching.store(false);

    if (watcherThread.joinable()) {
        watcherThread.join();
    }

    cleanupInotify();
    logger.info("ConfigWatcher: Stopped watching configuration file");
}

std::shared_ptr<std::vector<CronJob>> ConfigWatcher::getJobs() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return currentJobs;
}

bool ConfigWatcher::isConfigValid() const {
    std::lock_guard<std::mutex> lock(cacheMutex);
    return currentJobs && !currentJobs->empty();
}

bool ConfigWatcher::forceReload() {
    return validateAndLoadConfig();
}

bool ConfigWatcher::initializeInotify() {
    inotifyFd = inotify_init1(IN_NONBLOCK);
    if (inotifyFd == -1) {
        logger.error("ConfigWatcher: Failed to initialize inotify: " + std::string(strerror(errno)));
        return false;
    }

    // Watch the directory (not the file) so we keep receiving events even
    // when the config file is replaced via a rename (atomic-save pattern
    // used by most editors). IN_CREATE and IN_MOVED_TO catch the rename;
    // IN_MODIFY/IN_CLOSE_WRITE catch in-place writes.
    watchDescriptor = inotify_add_watch(inotifyFd, watchDir.c_str(),
                                       IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);

    if (watchDescriptor == -1) {
        logger.error("ConfigWatcher: Failed to add watch for " + watchDir + ": " + std::string(strerror(errno)));
        close(inotifyFd);
        inotifyFd = -1;
        return false;
    }

    return true;
}

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

void ConfigWatcher::watcherLoop() {
    logger.info("ConfigWatcher: Watcher thread started");

    // Buffer sized to hold several inotify events with reasonably long filenames.
    const size_t bufferSize = 4096;
    char buffer[bufferSize];

    while (!shouldStop.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(inotifyFd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1;  // Wake up periodically to check shouldStop
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
            continue; // Timeout, nothing to do this iteration.
        }

        if (!FD_ISSET(inotifyFd, &readfds)) {
            continue;
        }

        ssize_t bytesRead = read(inotifyFd, buffer, bufferSize);
        if (bytesRead == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                logger.error("ConfigWatcher: read() failed: " + std::string(strerror(errno)));
                break;
            }
            continue;
        }
        if (bytesRead <= 0) {
            continue;
        }

        // The directory watch fires for every file inside it - only react
        // to events that actually concern our jobs.json.
        bool relevantChange = false;
        size_t offset = 0;
        while (offset < (size_t)bytesRead) {
            auto* event = reinterpret_cast<struct inotify_event*>(buffer + offset);
            if (event->len > 0 && watchedFilename == event->name) {
                relevantChange = true;
            }
            offset += sizeof(struct inotify_event) + event->len;
        }

        if (!relevantChange) {
            continue;
        }

        logger.info("ConfigWatcher: Configuration file changed, reloading...");

        // Small delay so a multi-write save (some editors write in chunks)
        // has finished before we try to parse the file.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (validateAndLoadConfig()) {
            logger.success("ConfigWatcher: Configuration reloaded successfully");
        } else {
            logger.error("ConfigWatcher: Failed to reload configuration - keeping old version");
        }
    }

    logger.info("ConfigWatcher: Watcher thread stopped");
}

bool ConfigWatcher::validateAndLoadConfig() {
    try {
        std::string errorMsg;
        if (!JobConfig::validateJobsFile(configPath, errorMsg)) {
            logger.error("ConfigWatcher: Configuration validation failed: " + errorMsg);
            return false;
        }

        auto newJobs = loadJobsFromFile();
        if (!newJobs) {
            logger.error("ConfigWatcher: Failed to load new configuration");
            return false;
        }

        for (const auto& job : *newJobs) {
            if (job.command.empty()) {
                logger.error("ConfigWatcher: Invalid job found - empty command");
                return false;
            }
            if (job.description.empty()) {
                logger.warning("ConfigWatcher: Job with empty description: " + job.command);
            }
        }

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

std::shared_ptr<std::vector<CronJob>> ConfigWatcher::loadJobsFromFile() {
    try {
        std::ifstream testFile(configPath);
        if (!testFile.is_open()) {
            logger.error("ConfigWatcher: Configuration file does not exist or is not readable: " + configPath);
            return nullptr;
        }
        testFile.close();

        std::vector<CronJob> jobs = JobConfig::loadJobs(configPath);

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