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
 *
 * Each threshold is an optional string in the form "<NN%" or ">NN%"
 * (or "<NN" / ">NN" for load average, which has no unit).
 * An empty string means "no condition set for this metric".
 */
struct JobConditions {
    std::string cpu_threshold;      // CPU usage threshold (e.g., "<70%")
    std::string ram_threshold;      // RAM usage threshold (e.g., "<85%")
    std::string loadavg_threshold;  // Load average threshold (e.g., "<2.5")
    std::map<std::string, std::string> disk_thresholds; // Disk usage per mount path

    JobConditions() = default;
};

/**
 * STRUCT: Cron Schedule (cron-like syntax, stored as raw strings)
 *
 * Each field accepts: a fixed number ("5"), a wildcard ("*"), or an
 * interval ("* /N"). Only minute and hour support the "* /N" interval form.
 */
struct CronSchedule {
    std::string minute;        // Minute field (0-59, *, */N)
    std::string hour;          // Hour field (0-23, *, */N)
    std::string day_of_month;  // Day of month (1-31, *)
    std::string month;         // Month (1-12, *)
    std::string day_of_week;   // Day of week (0-6, *, or ranges like "1-5")
};

/**
 * STRUCT: Cron Job Definition
 *
 * "schedule" is the source of truth, parsed from JSON. The remaining
 * fields are a resolved/legacy representation used by CronEngine for
 * fast matching, derived from "schedule" at load time.
 *
 * Convention for minute/hour resolved values:
 *   -1  -> "*"        (matches any value)
 *   -2  -> "* /N"       (matches when value % N == 0; N stored in *_interval)
 *  >=0  -> exact match required
 */
struct CronJob {
    std::string description;    // Job description
    CronSchedule schedule;      // Cron-like schedule (raw, as read from JSON)
    std::string command;        // Command to execute
    JobConditions conditions;   // Optional system-resource execution conditions

    // Resolved schedule fields (derived from "schedule" - see convention above)
    int hour;
    int minute;
    int minute_interval; // Valid only when minute == -2 ("*/N" pattern)
    int hour_interval;   // Valid only when hour == -2 ("*/N" pattern)

    CronFrequency frequency; // Derived from day_of_month / day_of_week / month
    int day_param;           // Day of month or day of week, depending on frequency
    int month_param;         // Month, used by YEARLY frequency
};

#endif // CRON_TYPES_H