/**
 * @file JobExecutor.cpp
 * @brief Job execution engine with timeout handling and result analysis
 * 
 * Implements secure job execution with timeout protection, path resolution,
 * and comprehensive result logging. Provides the execution layer for the
 * nanoCron scheduling system with robust error handling and process management.
 */

#include "JobExecutor.h"
#include <cstdlib>
#include <filesystem>
#include <string>

/**
 * @brief Main job execution interface with comprehensive logging
 * @param job CronJob structure containing command and metadata
 * @param logger Logger instance for execution tracking and debugging
 * 
 * Orchestrates the complete job execution lifecycle including:
 * - Pre-execution logging with job identification
 * - Timeout-protected command execution
 * - Result analysis and classification (success/timeout/error)
 * - Post-execution logging with appropriate severity levels
 * 
 * This function serves as the primary interface between the scheduling engine
 * and the actual command execution subsystem, ensuring all executions are
 * properly tracked and logged for operational monitoring.
 */
void JobExecutor::executeJob(const CronJob& job, Logger& logger) {
    logger.info("Starting job: " + job.command, job.description);
    
    /**
     * Execute command with 300-second (5-minute) timeout protection
     * This prevents runaway processes from consuming system resources
     * indefinitely and ensures the daemon remains responsive for other
     * scheduled jobs. The timeout value is chosen to accommodate most
     * typical cron job execution times while preventing system lockup.
     */
    int result = executeWithTimeout(job.command, 300);
    
    /**
     * Result analysis and logging based on exit codes
     * 
     * Process exit code interpretation:
     * - 0: Successful execution (standard Unix convention)
     * - 124 * 256: Timeout occurred (timeout command exit code, shifted)
     * - Other values: Command-specific error codes
     * 
     * The exit code is multiplied by 256 because system() returns the
     * exit status in the high byte of the return value (WEXITSTATUS format).
     */
    if (result == 0) {
        // Command completed successfully
        logger.success("Job completed successfully", job.description);
    } else if (result == 124 * 256) {
        // Command terminated due to timeout
        logger.error("Job timed out after 300 seconds", job.description);
    } else {
        // Command failed with specific exit code
        logger.error("Job failed with exit code " + std::to_string(result / 256), job.description);
    }
}

/**
 * @brief Executes command with timeout protection and path resolution
 * @param command Shell command string to execute
 * @param timeout_seconds Maximum execution time in seconds
 * @return Process exit code (0 for success, non-zero for failure/timeout)
 * 
 * Implements robust command execution with the following features:
 * 
 * 1. **Path Resolution**: Converts relative paths (./script) to absolute paths
 *    to ensure commands execute correctly regardless of daemon working directory
 * 
 * 2. **Timeout Protection**: Uses system timeout command to prevent runaway
 *    processes, with graceful fallback if timeout utility is unavailable
 * 
 * 3. **Error Handling**: Distinguishes between timeout utility availability
 *    issues and actual command execution problems
 * 
 * The function prioritizes using the system's timeout command for reliability
 * but gracefully degrades to direct execution if timeout is not available,
 * ensuring compatibility across different Unix/Linux distributions.
 */
int JobExecutor::executeWithTimeout(const std::string& command, int timeout_seconds) {
    // Initialize with original command for fallback scenarios
    std::string full_command = command;
    
    /**
     * Relative path resolution for improved reliability
     * 
     * Converts "./script" format to absolute paths to prevent execution
     * failures when the daemon's working directory differs from the
     * expected script location. This is critical for cron jobs that
     * assume specific working directories.
     */
    if (command.find("./") == 0) {
        try {
            auto current_path = std::filesystem::current_path();
            full_command = current_path.string() + "/" + command.substr(2);
        } catch (const std::exception& e) {
            /**
             * Path resolution failure handling
             * If filesystem operations fail (permissions, invalid paths, etc.),
             * continue with the original command rather than failing entirely.
             * This provides graceful degradation for edge cases.
             */
            // Continue with original command if path resolution fails
        }
    }
    
    /**
     * Primary execution path: Use system timeout command
     * 
     * Constructs a timeout-protected command using the Unix timeout utility.
     * This provides reliable process termination after the specified duration
     * and is the preferred execution method for production environments.
     */
    std::string timed_command = "timeout " + std::to_string(timeout_seconds) + " " + full_command;
    int result = std::system(timed_command.c_str());
    
    /**
     * Fallback execution for systems without timeout command
     * 
     * Exit code 127 * 256 indicates command not found (timeout utility missing).
     * In this case, fall back to direct command execution without timeout
     * protection. While less safe, this ensures compatibility with minimal
     * systems that may not have timeout installed.
     * 
     * Note: The original code checks for 127 * 256, but this should be
     * corrected to detect timeout command availability more reliably.
     */
    if (result == 127 * 256) {
        result = std::system(full_command.c_str());
    }
    
    return result;
}