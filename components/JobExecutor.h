#ifndef JOB_EXECUTOR_H
#define JOB_EXECUTOR_H

#include "CronTypes.h"
#include "Logger.h"

/**
 * JobExecutor Class - Handles job execution
 * 
 * Manages the actual execution of scheduled jobs with
 * timeout handling, error management, and detailed logging.
 */
class JobExecutor {
public:
    /**
     * Execute a cron job
     * 
     * @param job The job to execute
     * @param logger Logger instance for output
     */
    static void executeJob(const CronJob& job, Logger& logger);
    
private:
    /**
     * Execute job with timeout and advanced error handling
     * 
     * @param command Command to execute
     * @param timeout_seconds Maximum execution time (default: 5 minutes)
     * @return Command exit code
     */
    static int executeWithTimeout(const std::string& command, int timeout_seconds = 300);
};

#endif // JOB_EXECUTOR_H