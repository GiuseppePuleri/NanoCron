/**
 * @file JobConfig.cpp
 * @brief JSON-based cron job configuration parser with performance optimizations
 * 
 * Handles loading, parsing, validation, and saving of cron job configurations
 * from JSON files. Includes move semantics optimizations and comprehensive
 * validation for robust configuration management.
 */

#include "JobConfig.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <map>

using json = nlohmann::json;

/**
 * Loads and parses cron jobs from JSON file
 * @param filename Path to JSON configuration file
 * @return Vector of parsed CronJob objects, empty on failure
 */
std::vector<CronJob> JobConfig::loadJobs(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Invalid jobs file" << std::endl;
        return {};
    }
    
    try {
        json j;
        file >> j;
        file.close();
        
        // OPTIMIZATION: Direct parsing without string conversion intermediate step
        return parseJobsFromJson(std::move(j));
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        std::cerr << "Using fallback configuration..." << std::endl;
        file.close();
        std::cerr << "Invalid jobs file" << std::endl;
        return {};
    }
}

/**
 * NEW: Direct parsing from nlohmann::json object (overload)
 * @param j JSON object to parse (moved for efficiency)
 * @return Vector of parsed CronJob objects
 * @note Uses move semantics throughout for optimal performance
 */
std::vector<CronJob> JobConfig::parseJobsFromJson(json&& j) {
    std::vector<CronJob> jobs;
    
    try {
        // Validate JSON structure contains required 'jobs' array
        if (!j.contains("jobs") || !j["jobs"].is_array()) {
            throw std::runtime_error("JSON must contain 'jobs' array");
        }
        
        const auto& jobs_array = j["jobs"];
        
        // OPTIMIZATION 1: Pre-allocate vector capacity to avoid reallocations
        jobs.reserve(jobs_array.size());
        
        // Parse each job definition
        for (auto&& job_json : jobs_array) {  // OPTIMIZATION 2: Universal reference for move
            CronJob job;
            
            // OPTIMIZATION 3: Move semantics for string fields to avoid copies
            // Parse required fields with validation
            if (job_json.contains("description") && job_json["description"].is_string()) {
                job.description = std::move(job_json["description"].get_ref<std::string&>());
            } else {
                throw std::runtime_error("Job missing required 'description' field");
            }
            
            if (job_json.contains("command") && job_json["command"].is_string()) {
                job.command = std::move(job_json["command"].get_ref<std::string&>());
            } else {
                throw std::runtime_error("Job missing required 'command' field");
            }
            
            // Parse cron schedule specification with move semantics
            if (job_json.contains("schedule") && job_json["schedule"].is_object()) {
                auto& schedule = job_json["schedule"];
                
                // OPTIMIZATION 4: Move semantics for schedule fields (avoid string copies)
                job.schedule.minute = schedule.contains("minute") ? 
                    std::move(schedule["minute"].get_ref<std::string&>()) : "*";
                job.schedule.hour = schedule.contains("hour") ? 
                    std::move(schedule["hour"].get_ref<std::string&>()) : "*";
                job.schedule.day_of_month = schedule.contains("day_of_month") ? 
                    std::move(schedule["day_of_month"].get_ref<std::string&>()) : "*";
                job.schedule.month = schedule.contains("month") ? 
                    std::move(schedule["month"].get_ref<std::string&>()) : "*";
                job.schedule.day_of_week = schedule.contains("day_of_week") ? 
                    std::move(schedule["day_of_week"].get_ref<std::string&>()) : "*";
            } else {
                throw std::runtime_error("Job missing required 'schedule' object");
            }
            
            // Parse optional execution conditions with move semantics
            if (job_json.contains("conditions") && job_json["conditions"].is_object()) {
                auto& conditions = job_json["conditions"];
                
                // OPTIMIZATION 5: Move semantics for condition thresholds
                if (conditions.contains("cpu")) {
                    job.conditions.cpu_threshold = std::move(conditions["cpu"].get_ref<std::string&>());
                }
                if (conditions.contains("ram")) {
                    job.conditions.ram_threshold = std::move(conditions["ram"].get_ref<std::string&>());
                }
                if (conditions.contains("loadavg")) {
                    job.conditions.loadavg_threshold = std::move(conditions["loadavg"].get_ref<std::string&>());
                }
                
                // Parse disk space conditions with move semantics
                if (conditions.contains("disk") && conditions["disk"].is_object()) {
                    for (auto&& [path, threshold] : conditions["disk"].items()) {
                        job.conditions.disk_thresholds[std::move(path)] = std::move(threshold.get_ref<std::string&>());
                    }
                }
            }
            
            // Convert modern JSON schedule format to legacy internal representation
            parseScheduleToLegacyFormat(job);
            
            // OPTIMIZATION 6: emplace_back instead of push_back (construct in-place)
            jobs.emplace_back(std::move(job));
            
            std::cout << "Loaded job: " << jobs.back().description << " [" << jobs.back().command << "]" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        std::cerr << "Invalid jobs file" << std::endl;
        return {};
    }
    
    if (jobs.empty()) {
        std::cerr << "No valid jobs found in JSON. Using fallback..." << std::endl;
        std::cerr << "Invalid jobs file" << std::endl;
        return {};
    }
    
    return jobs;
}

/**
 * Legacy compatibility method - parses from JSON string
 * @param json_string JSON configuration as string
 * @return Vector of parsed CronJob objects
 * @note Maintained for backward compatibility, delegates to optimized version
 */
std::vector<CronJob> JobConfig::parseJobsFromJson(const std::string& json_string) {
    try {
        // OPTIMIZATION 7: Direct parsing without intermediate copy
        json j = json::parse(json_string);
        return parseJobsFromJson(std::move(j));
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        std::cerr << "Invalid jobs file" << std::endl;
        return {};
    }
}

/**
 * Converts modern JSON schedule format to legacy internal representation
 * @param job CronJob object to populate with legacy fields
 * @note Handles cron expression parsing with validation and safe fallbacks
 */
void JobConfig::parseScheduleToLegacyFormat(CronJob& job) {
    // OPTIMIZATION 8: Cache common strings to avoid repeated string comparisons
    static const std::string ASTERISK = "*";
    static const std::string INTERVAL_PREFIX = "*/";
    
    // Parse minute field with validation
    if (job.schedule.minute == ASTERISK) {
        job.minute = -1; // Special value for "any minute"
    } else if (job.schedule.minute.compare(0, INTERVAL_PREFIX.length(), INTERVAL_PREFIX) == 0) {
        // Handle interval notation (*/X format for every X minutes)
        job.minute = -2; // Special value for interval
        // TODO: Store interval value for proper handling
    } else {
        // OPTIMIZATION 9: Fast integer parsing with comprehensive error handling
        try {
            job.minute = std::stoi(job.schedule.minute);
            // Range validation for cron minute field (0-59)
            if (job.minute < 0 || job.minute > 59) {
                job.minute = 0; // Safe fallback
            }
        } catch (const std::exception&) {
            job.minute = 0; // Default fallback for invalid input
        }
    }
    
    // Parse hour field with same optimization pattern
    if (job.schedule.hour == ASTERISK) {
        job.hour = -1; // Special value for "any hour"
    } else {
        try {
            job.hour = std::stoi(job.schedule.hour);
            // Range validation for cron hour field (0-23)
            if (job.hour < 0 || job.hour > 23) {
                job.hour = 0; // Safe fallback
            }
        } catch (const std::exception&) {
            job.hour = 0; // Default fallback
        }
    }
    
    // OPTIMIZATION 10: Determine frequency with early returns (avoid unnecessary checks)
    // Weekly frequency (day of week specified)
    if (job.schedule.day_of_week != ASTERISK) {
        job.frequency = CronFrequency::WEEKLY;
        try {
            job.day_param = std::stoi(job.schedule.day_of_week);
            if (job.day_param < 0 || job.day_param > 6) {
                job.day_param = 0; // Sunday as fallback
            }
        } catch (const std::exception&) {
            job.day_param = 0;
        }
        return; // Early exit for performance
    }
    
    // Monthly frequency (day of month specified)
    if (job.schedule.day_of_month != ASTERISK) {
        job.frequency = CronFrequency::MONTHLY;
        try {
            job.day_param = std::stoi(job.schedule.day_of_month);
            if (job.day_param < 1 || job.day_param > 31) {
                job.day_param = 1; // First day as fallback
            }
        } catch (const std::exception&) {
            job.day_param = 1;
        }
        return; // Early exit
    }
    
    // Yearly frequency (month specified)
    if (job.schedule.month != ASTERISK) {
        job.frequency = CronFrequency::YEARLY;
        try {
            job.day_param = std::stoi(job.schedule.day_of_month);
            job.month_param = std::stoi(job.schedule.month);
            // Validate date ranges
            if (job.day_param < 1 || job.day_param > 31) job.day_param = 1;
            if (job.month_param < 1 || job.month_param > 12) job.month_param = 1;
        } catch (const std::exception&) {
            job.day_param = 1;
            job.month_param = 1;
        }
        return; // Early exit
    }
    
    // Default case: daily frequency
    job.frequency = CronFrequency::DAILY;
    job.day_param = 0;
    job.month_param = 0;
}

/**
 * Evaluates job execution conditions against current system state
 * @param conditions JobConditions object containing thresholds
 * @return true if all conditions are met for job execution
 * @note Currently returns true (stub implementation) - TODO: implement system monitoring
 */
bool JobConfig::checkJobConditions(const JobConditions& conditions) {
    // TODO: Implement comprehensive system condition checking
    // This would check CPU usage, RAM consumption, disk space, load average, etc.
    // For now, return true (always execute jobs)
    
    // Example implementation outline:
    // - Parse threshold strings (e.g., ">90%", "<95%")
    // - Get current system statistics via /proc filesystem
    // - Compare current values with configured thresholds
    // - Return false if any condition is not met
    
    return true;
}

/**
 * Saves job configuration to JSON file
 * @param jobs Vector of CronJob objects to serialize
 * @param filename Output file path
 * @return true if save operation successful
 */
bool JobConfig::saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename) {
    try {
        json config;
        config["jobs"] = json::array();
        
        // OPTIMIZATION 11: Reserve space in JSON array to avoid reallocations
        config["jobs"].get_ref<json::array_t&>().reserve(jobs.size());
        
        // Serialize each job to JSON format
        for (const CronJob& job : jobs) {
            json job_json;
            job_json["description"] = job.description;
            job_json["command"] = job.command;
            
            // Serialize schedule specification
            job_json["schedule"]["minute"] = job.schedule.minute;
            job_json["schedule"]["hour"] = job.schedule.hour;
            job_json["schedule"]["day_of_month"] = job.schedule.day_of_month;
            job_json["schedule"]["month"] = job.schedule.month;
            job_json["schedule"]["day_of_week"] = job.schedule.day_of_week;
            
            // Conditionally serialize conditions (reduce JSON size for empty conditions)
            if (!job.conditions.cpu_threshold.empty() || 
                !job.conditions.ram_threshold.empty() || 
                !job.conditions.loadavg_threshold.empty() || 
                !job.conditions.disk_thresholds.empty()) {
                
                job_json["conditions"] = json::object();
                
                if (!job.conditions.cpu_threshold.empty()) {
                    job_json["conditions"]["cpu"] = job.conditions.cpu_threshold;
                }
                if (!job.conditions.ram_threshold.empty()) {
                    job_json["conditions"]["ram"] = job.conditions.ram_threshold;
                }
                if (!job.conditions.loadavg_threshold.empty()) {
                    job_json["conditions"]["loadavg"] = job.conditions.loadavg_threshold;
                }
                if (!job.conditions.disk_thresholds.empty()) {
                    job_json["conditions"]["disk"] = job.conditions.disk_thresholds;
                }
            }
            
            config["jobs"].emplace_back(std::move(job_json));
        }
        
        // Write formatted JSON to file
        std::ofstream file(filename);
        if (file.is_open()) {
            file << config.dump(2); // Pretty-print with 2-space indentation
            file.close();
            return true;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving JSON: " << e.what() << std::endl;
    }
    
    return false;
}

/**
 * Validates JSON configuration file structure and syntax
 * @param filename Path to configuration file
 * @param errorMsg Output parameter for detailed error description
 * @return true if file is valid and parseable
 * @note Performs both syntax and semantic validation
 */
bool JobConfig::validateJobsFile(const std::string& filename, std::string& errorMsg) {
    try {
        // Check file accessibility
        std::ifstream file(filename);
        if (!file.is_open()) {
            errorMsg = "Cannot open file: " + filename;
            return false;
        }
        
        // OPTIMIZATION 12: Efficient file reading with pre-allocation
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        if (fileSize == 0) {
            errorMsg = "File is empty: " + filename;
            file.close();
            return false;
        }
        
        // Pre-allocate string for entire file content
        std::string content;
        content.reserve(fileSize);
        
        // Read entire file in single operation
        content.assign((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
        file.close();
        
        // Quick structural validation before full JSON parsing
        if (!isValidJobsJson(content)) {
            errorMsg = "Invalid JSON structure in file: " + filename;
            return false;
        }
        
        // Full JSON syntax validation
        json j = json::parse(content);
        
        // Validate required JSON structure
        if (!j.contains("jobs") || !j["jobs"].is_array()) {
            errorMsg = "JSON must contain 'jobs' array";
            return false;
        }
        
        // Semantic validation of job structure
        for (const auto& job_json : j["jobs"]) {
            if (!job_json.contains("command") || !job_json["command"].is_string()) {
                errorMsg = "Job missing required 'command' field";
                return false;
            }
            
            if (!job_json.contains("description") || !job_json["description"].is_string()) {
                errorMsg = "Job missing required 'description' field";
                return false;
            }
            
            if (!job_json.contains("schedule") || !job_json["schedule"].is_object()) {
                errorMsg = "Job missing required 'schedule' object";
                return false;
            }
        }
        
        return true;
        
    } catch (const json::parse_error& e) {
        errorMsg = "JSON parse error: " + std::string(e.what());
        return false;
    } catch (const std::exception& e) {
        errorMsg = "Validation error: " + std::string(e.what());
        return false;
    }
}

/**
 * Fast JSON structure validation without full parsing
 * @param json_string JSON content as string
 * @return true if JSON has valid structure for jobs configuration
 * @note Performs lightweight validation before expensive full parsing
 */
bool JobConfig::isValidJobsJson(const std::string& json_string) {
    try {
        // OPTIMIZATION 13: Quick sanity checks without full JSON parsing
        if (json_string.empty()) {
            return false;
        }
        
        // Size limit check (prevent processing of extremely large files)
        if (json_string.size() > 1024 * 1024) { // 1MB limit
            return false;
        }
        
        // Must contain basic required structure
        if (json_string.find("\"jobs\"") == std::string::npos) {
            return false;
        }
        
        // OPTIMIZATION 14: Efficient brace/bracket balance checking
        int braceCount = 0;
        int bracketCount = 0;
        bool inString = false;
        bool escaped = false;
        
        // Single-pass validation of JSON structure
        for (size_t i = 0; i < json_string.size(); ++i) {
            char c = json_string[i];
            
            if (escaped) {
                escaped = false;
                continue;
            }
            
            if (c == '\\' && inString) {
                escaped = true;
                continue;
            }
            
            if (c == '"') {
                inString = !inString;
                continue;
            }
            
            // Track brace/bracket balance outside of strings
            if (!inString) {
                switch (c) {
                    case '{': braceCount++; break;
                    case '}': braceCount--; break;
                    case '[': bracketCount++; break;
                    case ']': bracketCount--; break;
                }
                
                // Early exit if structure becomes unbalanced
                if (braceCount < 0 || bracketCount < 0) {
                    return false;
                }
            }
        }
        
        // Final balance check - all braces and brackets must be matched
        return (braceCount == 0 && bracketCount == 0 && !inString);
        
    } catch (const std::exception&) {
        return false;
    }
}