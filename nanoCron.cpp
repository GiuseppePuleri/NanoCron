/******************************************************************************************
*       _   __                  ______                                                    *
*      / | / /___ _____  ____  / ____/________  ____                                      *
*     /  |/ / __ `/ __ \/ __ \/ /   / ___/ __ \/ __ \           Author: Giuseppe Puleri   *
*    / /|  / /_/ / / / / /_/ / /___/ /  / /_/ / / / /           License:  BSD 2-clause    *
*   /_/ |_/\__,_/_/ /_/\____/\____/_/   \____/_/ /_/            For: Linux systems        *
*   v: 2.0.0                                                                              *
*                                                                                         *
/*****************************************************************************************/

#include <iostream>
#include <ctime>
#include <chrono>
#include <thread>
#include <map>
#include <vector>

// Import modular components
#include "components/Logger.h"
#include "components/CronTypes.h"
#include "components/JobConfig.h"
#include "components/CronEngine.h"
#include "components/JobExecutor.h"

// Global logger instance (silent mode)
Logger logger;

int main() {
    // Silent startup - only log to file, not console
    logger.setSilentMode(true);
    logger.info("=== NANOCRON DAEMON STARTED ===");
    
    // Load job configuration from JSON file
    std::vector<CronJob> jobs = JobConfig::loadJobs("jobs.json");
    
    // Log loaded configuration (file only)
    logger.info("Loaded " + std::to_string(jobs.size()) + " jobs");
    for (const auto& job : jobs) {
        logger.info("Job: " + job.description + " [" + job.command + "]");
    }
    
    // Map to track last executions (prevents duplicates)
    std::map<std::string, std::pair<int, int>> last_execution;
    
    // Variables for periodic maintenance
    int last_rotation_day = -1;
    int last_debug_hour = -1;
    
    // Main daemon loop (completely silent)
    while (true) {
        // Get current system time
        std::time_t now = std::time(nullptr);
        std::tm local_time;
        
        #ifdef _WIN32
            localtime_s(&local_time, &now);
        #else
            localtime_r(&now, &local_time);
        #endif
        
        // Daily log rotation at midnight
        if (local_time.tm_mday != last_rotation_day && 
            local_time.tm_hour == 0 && local_time.tm_min == 0) {
            logger.rotate_logs();
            last_rotation_day = local_time.tm_mday;
        }
        
        // Periodic debug info every 4 hours
        if (local_time.tm_hour != last_debug_hour && local_time.tm_hour % 4 == 0) {
            CronEngine::logSystemStatus(local_time, logger);
            last_debug_hour = local_time.tm_hour;
        }
        
        // Check and execute jobs
        for (const auto& job : jobs) {
            if (CronEngine::shouldRunJob(job, local_time, last_execution)) {
                JobExecutor::executeJob(job, logger);
                
                // Mark job as executed
                last_execution[job.command] = {local_time.tm_hour, local_time.tm_min};
            }
        }
        
        // Wait 20 seconds before next check
        std::this_thread::sleep_for(std::chrono::seconds(20));
    }
    
    return 0;
}