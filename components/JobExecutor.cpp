#include "JobExecutor.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

void JobExecutor::executeJob(const CronJob& job, Logger& logger) {
    logger.info("Starting job: " + job.command, job.description);
    
    auto start_time = std::chrono::steady_clock::now();
    int result = executeWithTimeout(job.command, 300); // 5 minutes timeout
    auto end_time = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Analyze result and log appropriately
    if (result == 0) {
        // Success
        logger.success("Job completed successfully in " + 
                      std::to_string(duration.count()) + " seconds", job.description);
    } else if (result == 124 * 256) {
        // Timeout
        logger.error("Job timed out after 300 seconds", job.description);
    } else {
        // Other errors
        logger.error("Job failed with exit code " + std::to_string(result / 256) + 
                    " after " + std::to_string(duration.count()) + " seconds", job.description);
    }
}

int JobExecutor::executeWithTimeout(const std::string& command, int timeout_seconds) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Resolve relative paths to absolute
    std::string full_command = command;
    if (command.find("./") == 0) {
        try {
            auto current_path = std::filesystem::current_path();
            full_command = current_path.string() + "/" + command.substr(2);
        } catch (const std::exception& e) {
            // Continue with original command if path resolution fails
        }
    }
    
    // Try to use system timeout command
    std::string timed_command = "timeout " + std::to_string(timeout_seconds) + " " + full_command;
    int result = std::system(timed_command.c_str());
    
    // Fallback if timeout command is not available
    if (result == 127 * 256) {
        result = std::system(full_command.c_str());
    }
    
    return result;
}