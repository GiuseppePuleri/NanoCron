#ifndef CRON_TYPES_H
#define CRON_TYPES_H

#include <string>
#include <map>

/**
 * ENUM: Supported execution frequencies
 */
enum class CronFrequency {
    DAILY,          // Every day at the same time
    WEEKLY,         // Once a week (specify the day)
    MONTHLY,        // Once a month (specify day of month)
    YEARLY,         // Once a year (specify day and month)
    WEEKDAY,        // Only on weekdays (Monday-Friday)
    WEEKEND         // Only on weekends (Saturday-Sunday)
};

/**
 * ENUM: Logging levels
 */
enum class LogLevel {
    DEBUG,      // Detailed information for debugging
    INFO,       // General information about operation
    WARNING,    // Warnings - anomalous but not critical situations
    ERROR,      // Errors - something went wrong
    SUCCESS     // Operations completed successfully
};

/**
 * STRUCT: System Conditions for Job Execution
 */
struct JobConditions {
    std::string cpu_threshold;      // CPU usage threshold (e.g., ">90%")
    std::string ram_threshold;      // RAM usage threshold (e.g., ">80%")
    std::string loadavg_threshold;  // Load average threshold (e.g., ">5")
    std::map<std::string, std::string> disk_thresholds; // Disk usage per path
    
    // Default constructor
    JobConditions() = default;
};

/**
 * STRUCT: Cron Schedule (supports cron-like syntax)
 */
struct CronSchedule {
    std::string minute;        // Minute field (0-59, *, */5, etc.)
    std::string hour;          // Hour field (0-23, *, etc.)
    std::string day_of_month;  // Day of month (1-31, *, etc.)
    std::string month;         // Month (1-12, *, etc.)
    std::string day_of_week;   // Day of week (0-7, *, etc.)
};

/**
 * STRUCT: Enhanced Cron Job Definition with JSON support
 */
struct CronJob {
    std::string description;    // Job description
    CronSchedule schedule;      // Cron-like schedule
    std::string command;        // Command to execute
    JobConditions conditions;   // Optional execution conditions
    
    // Legacy fields for backward compatibility
    int hour;               // Will be parsed from schedule.hour
    int minute;             // Will be parsed from schedule.minute
    CronFrequency frequency; // Will be determined from schedule
    int day_param;          // Will be parsed from schedule fields
    int month_param;        // Will be parsed from schedule fields
};

#endif // CRON_TYPES_H