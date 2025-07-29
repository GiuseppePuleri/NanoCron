#ifndef JOB_CONFIG_H
#define JOB_CONFIG_H

#include <vector>
#include "CronTypes.h"

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
     * Save jobs to JSON file
     * 
     * @param jobs Vector of jobs to save
     * @param filename Output JSON file path
     * @return true if successful
     */
    static bool saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename);
    
    /**
     * Create example JSON configuration file
     * 
     * @param filename Output file path
     */
    static void createExampleConfig(const std::string& filename = "jobs.json");

private:
    /**
     * Parse cron schedule string to legacy format for compatibility
     */
    static void parseScheduleToLegacyFormat(CronJob& job);
    
    /**
     * Check if system meets job conditions
     */
    static bool checkJobConditions(const JobConditions& conditions);
    
    /**
     * Get fallback jobs if JSON loading fails
     */
    static std::vector<CronJob> getFallbackJobs();
};

#endif // JOB_CONFIG_H