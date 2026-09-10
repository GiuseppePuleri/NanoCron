#ifndef JOB_CONFIG_H
#define JOB_CONFIG_H

#include <vector>
#include <string>
#include "CronTypes.h"
#include "json.hpp"
#include <sys/statvfs.h>

/**
 * JobConfig Class - Manages JSON-based job configuration
 *
 * Loads job definitions from JSON files and resolves their schedule into
 * the fast-matching fields used by CronEngine. Also provides the runtime
 * system-resource checks (CPU/RAM/load/disk) used to conditionally allow
 * a job to run.
 */
class JobConfig {
public:
    /**
     * Load jobs from a JSON configuration file.
     *
     * @param filename Path to JSON config file (default: "jobs.json")
     * @return Vector containing all successfully parsed jobs
     */
    static std::vector<CronJob> loadJobs(const std::string& filename = "jobs.json");

    /**
     * Parse a JSON string and return jobs.
     *
     * @param json_string JSON configuration as string
     * @return Vector containing parsed jobs
     */
    static std::vector<CronJob> parseJobsFromJson(const std::string& json_string);

    /**
     * Parse directly from an nlohmann::json object (move semantics),
     * avoiding a redundant JSON -> string -> JSON round trip.
     *
     * @param j JSON object (moved)
     * @return Vector containing parsed jobs
     */
    static std::vector<CronJob> parseJobsFromJson(nlohmann::json&& j);

    /**
     * Save jobs to a JSON file.
     *
     * @param jobs Vector of jobs to save
     * @param filename Output JSON file path
     * @return true if successful
     */
    static bool saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename);

    /**
     * Validate a JSON file before loading it (used by ConfigWatcher before
     * swapping in a new configuration, so a malformed file never replaces
     * a working one).
     *
     * @param filename Path to JSON file
     * @param errorMsg  Output error message if validation fails
     * @return true if the file is valid and parseable
     */
    static bool validateJobsFile(const std::string& filename, std::string& errorMsg);

    /**
     * Quick validation of JSON structure without full job parsing.
     *
     * @param json_string Raw JSON content
     * @return true if the JSON structure looks valid
     */
    static bool isValidJobsJson(const std::string& json_string);

    /**
     * Check whether current system conditions (CPU/RAM/load/disk) satisfy
     * a job's requirements.
     *
     * IMPORTANT: this must be called at *execution time* (right before a
     * job is about to run), not at configuration-load time - system load
     * changes constantly, and a job whose condition is not met right now
     * should simply be skipped for this run, not removed from the schedule.
     *
     * @param conditions JobConditions to evaluate against the live system
     * @return true if all specified conditions are currently met
     */
    static bool checkJobConditions(const JobConditions& conditions);

private:
    /**
     * Parse a cron schedule string ("0 9 * * 1-5") into CronSchedule.
     */
    static void parseScheduleFromString(CronJob& job, const std::string& schedule_str);

    /**
     * Resolve CronSchedule (strings) into the fast-matching legacy fields
     * (hour, minute, minute_interval, hour_interval, frequency, ...).
     */
    static void parseScheduleToLegacyFormat(CronJob& job);

    /**
     * Convert the legacy hour/minute/day/month fields back into a
     * CronSchedule (used when jobs are defined in the old flat format).
     */
    static void convertLegacyToSchedule(CronJob& job);

    /**
     * Parse a single schedule field that may be "*", a fixed number, or
     * a "* /N" interval (e.g. "* /15" meaning every 15 units).
     *
     * @param field    Raw field value from the schedule
     * @param value    Output: -1 for "*", -2 for "* /N", or the exact number
     * @param interval Output: N when value == -2, otherwise 0
     * @return true if the field was valid and parsed successfully
     */
    static bool parseIntervalField(const std::string& field, int& value, int& interval);

    // --- System monitoring helpers ---
    static float getCurrentCpuUsage();
    static float getCurrentRamUsage();
    static float getCurrentLoadAverage();
    static float getCurrentDiskUsage(const std::string& path);
    static bool evaluateThreshold(float currentValue, const std::string& threshold, const std::string& metricName);
};

#endif // JOB_CONFIG_H