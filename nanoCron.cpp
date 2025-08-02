/******************************************************************************************
*       _   __                  ______                                                    *
*      / | / /___ _____  ____  / ____/________  ____                                      *
*     /  |/ / __ `/ __ \/ __ \/ /   / ___/ __ \/ __ \           Author: Giuseppe Puleri   *
*    / /|  / /_/ / / / / /_/ / /___/ /  / /_/ / / / /           License:  BSD 2-clause    *
*   /_/ |_/\__,_/_/ /_/\____/\____/_/   \____/_/ /_/            For: Linux systems        *
*                                                                                         *
*                                                                                         *
/*****************************************************************************************/

#include <iostream>
#include <ctime>
#include <chrono>
#include <thread>
#include <map>
#include <vector>
#include <fstream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include <memory>

// Import modular components
#include "components/Logger.h"
#include "components/CronTypes.h"
#include "components/JobConfig.h"
#include "components/CronEngine.h"
#include "components/JobExecutor.h"
#include "components/ConfigWatcher.h"

/**
 * @brief Global variables for graceful shutdown management
 * 
 * These variables ensure thread-safe communication between the signal handler
 * and the main daemon loop, enabling clean termination of all subsystems.
 */
std::unique_ptr<ConfigWatcher> configWatcher;  // Auto-reload configuration manager
Logger* globalLogger = nullptr;                // Global logger instance for signal handling
std::atomic<bool> shouldExit{false};          // Thread-safe shutdown flag

/**
 * @brief Signal handler for graceful daemon shutdown
 * @param signal The received signal number (SIGTERM/SIGINT)
 * 
 * Handles system signals to initiate graceful shutdown sequence.
 * Uses atomic operations to ensure thread safety between signal context
 * and main thread execution.
 */
void signalHandler(int signal) {
    if (globalLogger) {
        globalLogger->info("Received signal " + std::to_string(signal) + ", shutting down gracefully...");
    }
    shouldExit.store(true);  // Atomic write for thread safety
}

/**
 * @brief Retrieves jobs.json configuration file path from environment config
 * @return Absolute path to jobs.json file or fallback default
 * 
 * Implements configuration path resolution by reading the config.env file.
 * This allows dynamic job configuration without recompilation when users
 * modify the jobs.json location. Falls back to "./jobs.json" if config
 * file is inaccessible or malformed.
 */
std::string getJobsJsonPath() {
    const std::string CONFIG_FILE = "/opt/nanoCron/init/config.env";
    std::ifstream configFile(CONFIG_FILE);
    
    if (!configFile.is_open()) {
        std::cerr << "WARNING: Cannot open config file: " + CONFIG_FILE << std::endl;
        std::cout << "INFO: Falling back to default: ./jobs.json" << std::endl;
        return "./jobs.json";
    }
    
    std::string line;
    // Parse config file line by line searching for jobs path declaration
    while (std::getline(configFile, line)) {
        if (line.find("ORIGINAL_JOBS_JSON_PATH=") == 0) {
            std::string path = line.substr(24);  // Remove prefix to extract path
            configFile.close();
            return path;
        }
    }
    
    configFile.close();
    return "./jobs.json";
}

/**
 * @brief Retrieves log file path from environment configuration
 * @return Absolute path to cron.log file or fallback default
 * 
 * Similar to getJobsJsonPath() but for log file location.
 * Enables flexible log file placement for different deployment scenarios
 * (system-wide vs local development).
 */
std::string getCronLogPath() {
    const std::string CONFIG_FILE = "/opt/nanoCron/init/config.env";
    std::ifstream configFile(CONFIG_FILE);
    
    if (!configFile.is_open()) {
        std::cerr << "WARNING: Cannot open config file: " + CONFIG_FILE << std::endl;
        std::cout << "INFO: Falling back to default: ./logs/cron.log" << std::endl;
        return "./logs/cron.log";
    }
    
    std::string line;
    while (std::getline(configFile, line)) {
        if (line.find("ORIGINAL_CRON_LOG_PATH=") == 0) {
            std::string path = line.substr(23);  // Remove "ORIGINAL_CRON_LOG_PATH=" prefix
            configFile.close();
            return path;
        }
    }
    
    configFile.close();
    return "./logs/cron.log";
}

/**
 * @brief Main daemon entry point and execution loop
 * @return Exit code (0 for successful termination)
 * 
 * Orchestrates the complete nanoCron daemon lifecycle:
 * 1. Signal handler registration for graceful shutdown
 * 2. Logger initialization with silent mode for daemon operation
 * 3. ConfigWatcher setup for automatic configuration reloading
 * 4. Main execution loop with job scheduling and system maintenance
 * 5. Graceful cleanup and resource deallocation
 */
int main() {
    // Setup signal handlers for graceful shutdown
    signal(SIGTERM, signalHandler);  // Handle systemd stop commands
    signal(SIGINT, signalHandler);   // Handle Ctrl+C during development
    
    // Initialize logging subsystem in silent mode (daemon operation)
    Logger logger(getCronLogPath());
    globalLogger = &logger;
    logger.setSilentMode(true);  // Suppress console output for daemon mode
    logger.info("=== NANOCRON DAEMON STARTED (v2.1.0) ===");
    
    // Log current working directory for debugging path resolution issues
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        logger.info("Working directory: " + std::string(cwd));
    }
    
    /**
     * Initialize ConfigWatcher for automatic configuration reloading
     * This replaces static job loading and enables runtime configuration changes
     * without daemon restart, improving operational flexibility.
     */
    std::string jobsPath = getJobsJsonPath();
    logger.info("Initializing ConfigWatcher for: " + jobsPath);
    
    configWatcher = std::make_unique<ConfigWatcher>(jobsPath, logger);
    
    // Attempt to start inotify-based file watching
    if (!configWatcher->startWatching()) {
        logger.error("Failed to start configuration watcher - continuing with static config");
    } else {
        logger.info("Configuration auto-reload enabled");
    }
    
    // Load and validate initial job configuration
    auto jobs = configWatcher->getJobs();
    if (!jobs || jobs->empty()) {
        logger.warning("No jobs loaded from configuration file");
    } else {
        logger.info("Initial load: " + std::to_string(jobs->size()) + " jobs");
        // Log each job for startup verification
        for (const auto& job : *jobs) {
            logger.info("Job: " + job.description + " [" + job.command + "]");
        }
    }
    
    /**
     * Job execution tracking map prevents duplicate executions within the same minute.
     * Key: job command string, Value: pair<hour, minute> of last execution
     * This is critical for preventing runaway job executions when the daemon
     * checks every 20 seconds but jobs are scheduled per minute.
     */
    std::map<std::string, std::pair<int, int>> last_execution;
    
    // Maintenance operation tracking variables
    int last_rotation_day = -1;   // Track daily log rotation
    int last_debug_hour = -1;     // Track periodic system status logging
    int config_check_counter = 0; // Counter for "no jobs" warning frequency
    
    logger.info("Entering main daemon loop");
    
    /**
     * Main daemon execution loop - runs continuously until shutdown signal
     * 
     * Loop operations (every 20 seconds):
     * 1. Get current system time with thread-safe localtime_r
     * 2. Perform daily maintenance (log rotation at midnight)
     * 3. Log periodic system status (every 4 hours)
     * 4. Retrieve current job configuration from ConfigWatcher
     * 5. Evaluate and execute scheduled jobs
     * 6. Handle error conditions (missing configuration)
     */
    while (!shouldExit.load()) {
        // Get current system time using thread-safe time functions
        std::time_t now = std::time(nullptr);
        std::tm local_time;
        
        #ifdef _WIN32
            localtime_s(&local_time, &now);      // Windows thread-safe version
        #else
            localtime_r(&now, &local_time);      // POSIX thread-safe version
        #endif
        
        /**
         * Daily log rotation at midnight (00:00)
         * Prevents log files from growing indefinitely and ensures
         * consistent log management across long-running daemon instances.
         */
        if (local_time.tm_mday != last_rotation_day && 
            local_time.tm_hour == 0 && local_time.tm_min == 0) {
            logger.rotate_logs();
            last_rotation_day = local_time.tm_mday;
        }
        
        /**
         * Periodic system status logging every 4 hours
         * Provides operational visibility and confirms daemon health
         * without overwhelming the log files.
         */
        if (local_time.tm_hour != last_debug_hour && local_time.tm_hour % 4 == 0) {
            CronEngine::logSystemStatus(local_time, logger);
            last_debug_hour = local_time.tm_hour;
        }
        
        /**
         * Retrieve current jobs from ConfigWatcher cache
         * The cache is automatically updated by the inotify watcher thread
         * when configuration files change, providing real-time config updates.
         */
        auto currentJobs = configWatcher->getJobs();
        
        if (currentJobs && !currentJobs->empty()) {
            // Iterate through all configured jobs and check execution conditions
            for (const auto& job : *currentJobs) {
                /**
                 * Job execution decision logic:
                 * - Check if current time matches job schedule
                 * - Verify job hasn't already executed this minute
                 * - Validate any system condition requirements
                 */
                if (CronEngine::shouldRunJob(job, local_time, last_execution)) {
                    JobExecutor::executeJob(job, logger);
                    
                    // Mark job as executed to prevent duplicate runs
                    last_execution[job.command] = {local_time.tm_hour, local_time.tm_min};
                }
            }
        } else {
            /**
             * Handle configuration unavailability
             * Log warnings periodically (every 5 minutes) to avoid log spam
             * while still alerting operators to configuration issues.
             */
            config_check_counter++;
            if (config_check_counter >= 15) { // 15 * 20 seconds = 5 minutes
                logger.warning("No jobs currently loaded from configuration");
                config_check_counter = 0;
            }
        }
        
        /**
         * Sleep for 20 seconds before next iteration
         * This interval provides good responsiveness for minute-based scheduling
         * while keeping CPU usage minimal (3 checks per minute maximum).
         */
        std::this_thread::sleep_for(std::chrono::seconds(20));
    }
    
    /**
     * Graceful shutdown sequence
     * Ensures all resources are properly cleaned up and background threads
     * are safely terminated before process exit.
     */
    logger.info("Shutting down nanoCron daemon...");
    
    if (configWatcher) {
        configWatcher->stopWatching();  // Stop inotify thread
        configWatcher.reset();          // Release resources
    }
    
    logger.info("=== NANOCRON DAEMON STOPPED ===");
    
    return 0;
}