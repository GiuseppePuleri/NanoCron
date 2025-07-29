#ifndef CRON_ENGINE_H
#define CRON_ENGINE_H

#include <map>
#include <string>
#include <ctime>
#include "CronTypes.h"
#include "Logger.h"

/**
 * CronEngine Class - Core scheduling logic
 * 
 * Contains all the logic for determining when jobs should run,
 * displaying schedules, and system status reporting.
 */
class CronEngine {
public:
    /**
     * Determines if a job should be executed now
     * 
     * @param job The job to check
     * @param local_time Current system time
     * @param last_exec Map of last executions to prevent duplicates
     * @return true if job should run
     */
    static bool shouldRunJob(const CronJob& job, 
                           const std::tm& local_time, 
                           const std::map<std::string, std::pair<int, int>>& last_exec);
    
    /**
     * Print job schedule information in human-readable format
     * 
     * @param job The job to display
     * @param logger Logger instance for output
     */
    static void printJobSchedule(const CronJob& job, Logger& logger);
    
    /**
     * Log current system status
     * 
     * @param local_time Current system time
     * @param logger Logger instance for output
     */
    static void logSystemStatus(const std::tm& local_time, Logger& logger);
    
private:
    /**
     * Get weekday name from number
     * 
     * @param wday Day number (0=Sunday, 1=Monday, etc.)
     * @return Day name as string
     */
    static std::string getWeekdayName(int wday);
};

#endif // CRON_ENGINE_H