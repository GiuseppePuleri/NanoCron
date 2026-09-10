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

#include "components/Logger.h"
#include "components/CronTypes.h"
#include "components/JobConfig.h"
#include "components/CronEngine.h"
#include "components/JobExecutor.h"
#include "components/ConfigWatcher.h"

/**
 * Globals used for signal-safe, thread-safe graceful shutdown.
 */
std::unique_ptr<ConfigWatcher> configWatcher;
Logger* globalLogger = nullptr;
std::atomic<bool> shouldExit{false};

void signalHandler(int signal) {
    if (globalLogger) {
        globalLogger->info("Received signal " + std::to_string(signal) + ", shutting down gracefully...");
    }
    shouldExit.store(true);
}

/**
 * Reads a "KEY=value" style entry from config.env.
 * Returns fallbackValue if the file or key is missing.
 */
static std::string readConfigEnvValue(const std::string& key, const std::string& fallbackValue) {
    const std::string CONFIG_FILE = "/opt/nanoCron/init/config.env";
    std::ifstream configFile(CONFIG_FILE);

    if (!configFile.is_open()) {
        std::cerr << "WARNING: Cannot open config file: " << CONFIG_FILE << std::endl;
        std::cout << "INFO: Falling back to default: " << fallbackValue << std::endl;
        return fallbackValue;
    }

    const std::string prefix = key + "=";
    std::string line;
    while (std::getline(configFile, line)) {
        if (line.compare(0, prefix.size(), prefix) == 0) {
            return line.substr(prefix.size());
        }
    }

    return fallbackValue;
}

std::string getJobsJsonPath() {
    return readConfigEnvValue("ORIGINAL_JOBS_JSON_PATH", "./jobs.json");
}

std::string getCronLogPath() {
    return readConfigEnvValue("ORIGINAL_CRON_LOG_PATH", "./logs/cron.log");
}

int main() {
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);

    Logger logger(getCronLogPath());
    globalLogger = &logger;
    logger.setSilentMode(true);
    logger.info("=== NANOCRON DAEMON STARTED ===");

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        logger.info("Working directory: " + std::string(cwd));
    }

    std::string jobsPath = getJobsJsonPath();
    logger.info("Initializing ConfigWatcher for: " + jobsPath);

    configWatcher = std::make_unique<ConfigWatcher>(jobsPath, logger);

    if (!configWatcher->startWatching()) {
        logger.error("Failed to start configuration watcher - continuing with static config");
    } else {
        logger.info("Configuration auto-reload enabled");
    }

    auto jobs = configWatcher->getJobs();
    if (!jobs || jobs->empty()) {
        logger.warning("No jobs loaded from configuration file");
    } else {
        logger.info("Initial load: " + std::to_string(jobs->size()) + " jobs");
        for (const auto& job : *jobs) {
            logger.info("Job: " + job.description + " [" + job.command + "]");
        }
    }

    // Tracks the last (hour, minute) each job command was executed in, to
    // avoid double-firing a job within the same minute (the loop below
    // polls every 20 seconds, i.e. up to 3 times per minute).
    std::map<std::string, std::pair<int, int>> last_execution;

    int last_rotation_day = -1;
    int last_debug_hour = -1;
    int config_check_counter = 0;

    logger.info("Entering main daemon loop");

    while (!shouldExit.load()) {
        std::time_t now = std::time(nullptr);
        std::tm local_time;

        #ifdef _WIN32
            localtime_s(&local_time, &now);
        #else
            localtime_r(&now, &local_time);
        #endif

        // Daily log rotation at midnight.
        if (local_time.tm_mday != last_rotation_day &&
            local_time.tm_hour == 0 && local_time.tm_min == 0) {
            logger.rotate_logs();
            last_rotation_day = local_time.tm_mday;
        }

        // Periodic "still alive" status log, every 4 hours.
        if (local_time.tm_hour != last_debug_hour && local_time.tm_hour % 4 == 0) {
            CronEngine::logSystemStatus(local_time, logger);
            last_debug_hour = local_time.tm_hour;
        }

        auto currentJobs = configWatcher->getJobs();

        if (currentJobs && !currentJobs->empty()) {
            for (const auto& job : *currentJobs) {
                if (!CronEngine::shouldRunJob(job, local_time, last_execution)) {
                    continue;
                }

                // System-resource conditions (CPU/RAM/load/disk) are checked
                // here, right before running the job - not when the config
                // file was loaded. System load changes constantly, so this
                // must be a fresh check every time a job is about to fire.
                if (!JobConfig::checkJobConditions(job.conditions)) {
                    logger.warning("Job skipped, system conditions not met", job.description);
                    // Still mark as "handled" for this minute so we don't
                    // re-evaluate (and re-log) it on every poll within the
                    // same minute; it will be reconsidered on its next
                    // scheduled occurrence.
                    last_execution[job.command] = {local_time.tm_hour, local_time.tm_min};
                    continue;
                }

                JobExecutor::executeJob(job, logger);
                last_execution[job.command] = {local_time.tm_hour, local_time.tm_min};
            }
        } else {
            // Warn about missing configuration at most once every 5 minutes
            // (15 polls * 20 seconds) to avoid flooding the log.
            config_check_counter++;
            if (config_check_counter >= 15) {
                logger.warning("No jobs currently loaded from configuration");
                config_check_counter = 0;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(20));
    }

    logger.info("Shutting down nanoCron daemon...");

    if (configWatcher) {
        configWatcher->stopWatching();
        configWatcher.reset();
    }

    logger.info("=== NANOCRON DAEMON STOPPED ===");
    return 0;
}