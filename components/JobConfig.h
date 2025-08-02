#ifndef JOB_CONFIG_H
#define JOB_CONFIG_H

#include <vector>
#include <string>
#include <sstream>
#include "CronTypes.h"
#include "json.hpp"
#include <sys/statvfs.h>
#include <sstream> 

/**
 * JobConfig Class - Manages JSON-based job configuration
 * 
 * This class loads job definitions from external JSON files,
 * making configuration management much more flexible and user-friendly.
 */
class JobConfig {
public:
    /**
     * Load jobs from JSON configuration file
     * 
     * @param filename Path to JSON config file (default: "jobs.json")
     * @return Vector containing all loaded jobs
     */
    static std::vector<CronJob> loadJobs(const std::string& filename = "jobs.json");
    
    /**
     * Parse JSON string and return jobs
     * 
     * @param json_string JSON configuration as string
     * @return Vector containing parsed jobs
     */
    static std::vector<CronJob> parseJobsFromJson(const std::string& json_string);
    
    /**
     * OTTIMIZZATO: Parse direttamente da nlohmann::json object (move semantics)
     * Evita doppio parsing JSON -> string -> JSON
     * 
     * @param j JSON object (moved)
     * @return Vector containing parsed jobs
     */
    static std::vector<CronJob> parseJobsFromJson(nlohmann::json&& j);
    
    /**
     * Save jobs to JSON file
     * 
     * @param jobs Vector of jobs to save
     * @param filename Output JSON file path
     * @return true if successful
     */
    static bool saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename);
    
    // NEW: Validation methods for auto-reload feature
    /**
     * Validate JSON file before loading
     * @param filename Path to JSON file
     * @param errorMsg Output error message if validation fails
     * @return true if file is valid and parseable
     */
    static bool validateJobsFile(const std::string& filename, std::string& errorMsg);
    
    /**
     * Quick validation of JSON structure without full parsing
     * @param json_string Raw JSON content
     * @return true if JSON structure looks valid
     */
    static bool isValidJobsJson(const std::string& json_string);

private:
    /**
     * Parse cron schedule string to CronSchedule structure
     */
    static void parseScheduleFromString(CronJob& job, const std::string& schedule_str);
    
    /**
     * Parse cron schedule to legacy format for compatibility
     */
    static void parseScheduleToLegacyFormat(CronJob& job);
    
    /**
     * Convert legacy format to new schedule structure
     */
    static void convertLegacyToSchedule(CronJob& job);
    
    /**
     * Check if system meets job conditions
     */
    static bool checkJobConditions(const JobConditions& conditions);

        /**
     * System monitoring helper functions
     */
    static float getCurrentCpuUsage();
    static float getCurrentRamUsage();
    static float getCurrentLoadAverage();
    static float getCurrentDiskUsage(const std::string& path);
    static bool evaluateThreshold(float currentValue, const std::string& threshold, const std::string& metricName);

};

#endif // JOB_CONFIG_H