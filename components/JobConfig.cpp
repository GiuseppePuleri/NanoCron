/**
 * @file JobConfig.cpp
 * @brief JSON-based job configuration management with validation
 */

#include "JobConfig.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

/**
 * Load jobs from JSON configuration file
 */
std::vector<CronJob> JobConfig::loadJobs(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open config file: " << filename << std::endl;
        return {};
    }
    
    try {
        nlohmann::json j;
        file >> j;
        return parseJobsFromJson(std::move(j));
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error in " << filename << ": " << e.what() << std::endl;
        return {};
    } catch (const std::exception& e) {
        std::cerr << "Error loading jobs from " << filename << ": " << e.what() << std::endl;
        return {};
    }
}

/**
 * OTTIMIZZATO: Parse direttamente da nlohmann::json object (move semantics)
 * Evita doppio parsing JSON -> string -> JSON
 */
std::vector<CronJob> JobConfig::parseJobsFromJson(nlohmann::json&& j) {
    std::vector<CronJob> jobs;
    
    if (!j.contains("jobs") || !j["jobs"].is_array()) {
        std::cerr << "Error: JSON must contain 'jobs' array" << std::endl;
        return {};
    }
    
    try {
        for (const auto& job_json : j["jobs"]) {
            CronJob job;
            
            // Required fields
            if (!job_json.contains("description") || !job_json.contains("command")) {
                std::cerr << "Warning: Skipping job missing required fields (description, command)" << std::endl;
                continue;
            }
            
            job.description = job_json["description"].get<std::string>();
            job.command = job_json["command"].get<std::string>();
            
            // Schedule parsing (new unified format)
            if (job_json.contains("schedule")) {
                const auto& sched = job_json["schedule"];
                
                // Support both string format ("0 9 * * 1-5") and object format
                if (sched.is_string()) {
                    // Parse cron string format
                    parseScheduleFromString(job, sched.get<std::string>());
                } else if (sched.is_object()) {
                    // Parse object format
                    job.schedule.minute = sched.value("minute", "*");
                    job.schedule.hour = sched.value("hour", "*");
                    job.schedule.day_of_month = sched.value("day_of_month", "*");
                    job.schedule.month = sched.value("month", "*");
                    job.schedule.day_of_week = sched.value("day_of_week", "*");
                }
                
                // Convert to legacy format for backward compatibility
                parseScheduleToLegacyFormat(job);
            } else {
                // Legacy format fallback
                job.hour = job_json.value("hour", -1);
                job.minute = job_json.value("minute", -1);
                job.day_param = job_json.value("day", -1);
                job.month_param = job_json.value("month", -1);
                
                // Convert legacy to new schedule format
                convertLegacyToSchedule(job);
            }
            
            // Job conditions (optional)
            if (job_json.contains("conditions")) {
                const auto& cond = job_json["conditions"];
                job.conditions.cpu_threshold = cond.value("cpu_threshold", "");
                job.conditions.ram_threshold = cond.value("ram_threshold", "");
                job.conditions.loadavg_threshold = cond.value("loadavg_threshold", "");
                
                // Parse disk thresholds
                if (cond.contains("disk_thresholds") && cond["disk_thresholds"].is_object()) {
                    for (const auto& [path, threshold] : cond["disk_thresholds"].items()) {
                        job.conditions.disk_thresholds[path] = threshold.get<std::string>();
                    }
                }
            }
            
            // Only add if conditions are met
            if (checkJobConditions(job.conditions)) {
                jobs.push_back(job);
            }
        }
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing job configuration: " << e.what() << std::endl;
        return {};
    }
    
    return jobs;
}

/**
 * Parse JSON string and return jobs
 */
std::vector<CronJob> JobConfig::parseJobsFromJson(const std::string& json_string) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_string);
        return parseJobsFromJson(std::move(j));
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return {};
    }
}

/**
 * Save jobs to JSON file with optimized structure
 */
bool JobConfig::saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename) {
    try {
        nlohmann::json config;
        config["jobs"] = nlohmann::json::array();
        
        // Reserve space for better performance
        config["jobs"].get_ref<nlohmann::json::array_t&>().reserve(jobs.size());
        
        for (const auto& job : jobs) {
            nlohmann::json job_json;
            job_json["description"] = job.description;
            job_json["command"] = job.command;
            
            // Use new schedule format
            nlohmann::json schedule_json;
            schedule_json["minute"] = job.schedule.minute;
            schedule_json["hour"] = job.schedule.hour;
            schedule_json["day_of_month"] = job.schedule.day_of_month;
            schedule_json["month"] = job.schedule.month;
            schedule_json["day_of_week"] = job.schedule.day_of_week;
            job_json["schedule"] = schedule_json;
            
            // Add conditions only if they're not default
            if (!job.conditions.cpu_threshold.empty() || 
                !job.conditions.ram_threshold.empty() || 
                !job.conditions.loadavg_threshold.empty() ||
                !job.conditions.disk_thresholds.empty()) {
                
                nlohmann::json conditions_json;
                
                if (!job.conditions.cpu_threshold.empty())
                    conditions_json["cpu_threshold"] = job.conditions.cpu_threshold;
                if (!job.conditions.ram_threshold.empty())
                    conditions_json["ram_threshold"] = job.conditions.ram_threshold;
                if (!job.conditions.loadavg_threshold.empty())
                    conditions_json["loadavg_threshold"] = job.conditions.loadavg_threshold;
                
                if (!job.conditions.disk_thresholds.empty()) {
                    nlohmann::json disk_json;
                    for (const auto& [path, threshold] : job.conditions.disk_thresholds) {
                        disk_json[path] = threshold;
                    }
                    conditions_json["disk_thresholds"] = disk_json;
                }
                
                job_json["conditions"] = conditions_json;
            }
            
            config["jobs"].push_back(job_json);
        }
        
        // Write to file with pretty formatting
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot create file: " << filename << std::endl;
            return false;
        }
        
        file << config.dump(2); // Indent with 2 spaces
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving jobs to " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

/**
 * Validate JSON file before loading
 */
bool JobConfig::validateJobsFile(const std::string& filename, std::string& errorMsg) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        errorMsg = "Cannot open file: " + filename;
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    try {
        nlohmann::json j = nlohmann::json::parse(content);
        
        // Basic structure validation
        if (!j.contains("jobs") || !j["jobs"].is_array()) {
            errorMsg = "JSON must contain 'jobs' array";
            return false;
        }
        
        // Validate each job
        for (const auto& job_json : j["jobs"]) {
            if (!job_json.contains("description") || !job_json.contains("command")) {
                errorMsg = "Each job must have 'description' and 'command' fields";
                return false;
            }
        }
        
        return true;
        
    } catch (const nlohmann::json::parse_error& e) {
        errorMsg = "JSON parse error: " + std::string(e.what());
        return false;
    } catch (const std::exception& e) {
        errorMsg = "Validation error: " + std::string(e.what());
        return false;
    }
}

/**
 * Quick validation of JSON structure without full parsing
 */
bool JobConfig::isValidJobsJson(const std::string& json_string) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_string);
        return j.contains("jobs") && j["jobs"].is_array();
    } catch (const nlohmann::json::parse_error&) {
        return false;
    }
}

/**
 * Parse cron schedule string to CronSchedule structure
 * Supports formats like "0 9 * * 1-5" (Monday-Friday at 9 AM)
 */
void JobConfig::parseScheduleFromString(CronJob& job, const std::string& schedule_str) {
    std::istringstream iss(schedule_str);
    std::string minute_str, hour_str, day_str, month_str, weekday_str;
    
    if (iss >> minute_str >> hour_str >> day_str >> month_str >> weekday_str) {
        job.schedule.minute = minute_str;
        job.schedule.hour = hour_str;
        job.schedule.day_of_month = day_str;
        job.schedule.month = month_str;
        job.schedule.day_of_week = weekday_str;
    } else {
        std::cerr << "Warning: Could not parse schedule string '" << schedule_str << "'" << std::endl;
    }
}

/**
 * Parse cron schedule to legacy format for compatibility
 */
void JobConfig::parseScheduleToLegacyFormat(CronJob& job) {
    try {
        job.minute = (job.schedule.minute == "*") ? -1 : std::stoi(job.schedule.minute);
        job.hour = (job.schedule.hour == "*") ? -1 : std::stoi(job.schedule.hour);
        job.day_param = (job.schedule.day_of_month == "*") ? -1 : std::stoi(job.schedule.day_of_month);
        job.month_param = (job.schedule.month == "*") ? -1 : std::stoi(job.schedule.month);
        
        // Determine frequency based on schedule
        if (job.schedule.day_of_week == "1-5") {
            job.frequency = CronFrequency::WEEKDAY;
        } else if (job.schedule.day_of_week == "0,6" || job.schedule.day_of_week == "6,0") {
            job.frequency = CronFrequency::WEEKEND;
        } else if (job.schedule.day_of_week != "*") {
            job.frequency = CronFrequency::WEEKLY;
        } else if (job.schedule.day_of_month != "*") {
            job.frequency = CronFrequency::MONTHLY;
        } else if (job.schedule.month != "*") {
            job.frequency = CronFrequency::YEARLY;
        } else {
            job.frequency = CronFrequency::DAILY;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not parse schedule fields: " << e.what() << std::endl;
        // Set defaults
        job.minute = -1;
        job.hour = -1;
        job.day_param = -1;
        job.month_param = -1;
        job.frequency = CronFrequency::DAILY;
    }
}

/**
 * Convert legacy format to new schedule structure
 */
void JobConfig::convertLegacyToSchedule(CronJob& job) {
    job.schedule.minute = (job.minute == -1) ? "*" : std::to_string(job.minute);
    job.schedule.hour = (job.hour == -1) ? "*" : std::to_string(job.hour);
    job.schedule.day_of_month = (job.day_param == -1) ? "*" : std::to_string(job.day_param);
    job.schedule.month = (job.month_param == -1) ? "*" : std::to_string(job.month_param);
    
    // Set day_of_week based on frequency
    switch (job.frequency) {
        case CronFrequency::WEEKDAY:
            job.schedule.day_of_week = "1-5";
            break;
        case CronFrequency::WEEKEND:
            job.schedule.day_of_week = "0,6";
            break;
        case CronFrequency::WEEKLY:
            // Would need more info to determine specific day
            job.schedule.day_of_week = "*";
            break;
        default:
            job.schedule.day_of_week = "*";
            break;
    }
}

/**
 * Check if system meets job conditions
 * @param conditions JobConditions object containing system resource thresholds
 * @return true if all conditions are met, false otherwise
 */
bool JobConfig::checkJobConditions(const JobConditions& conditions) {
    // If no conditions are specified, always allow execution
    if (conditions.cpu_threshold.empty() && 
        conditions.ram_threshold.empty() && 
        conditions.loadavg_threshold.empty() && 
        conditions.disk_thresholds.empty()) {
        return true;
    }
    
    // Check CPU usage condition
    if (!conditions.cpu_threshold.empty()) {
        float currentCpu = getCurrentCpuUsage();
        if (currentCpu < 0) {
            std::cerr << "Warning: Could not read CPU usage, ignoring CPU condition" << std::endl;
        } else {
            if (!evaluateThreshold(currentCpu, conditions.cpu_threshold, "CPU")) {
                return false;
            }
        }
    }
    
    // Check RAM usage condition
    if (!conditions.ram_threshold.empty()) {
        float currentRam = getCurrentRamUsage();
        if (currentRam < 0) {
            std::cerr << "Warning: Could not read RAM usage, ignoring RAM condition" << std::endl;
        } else {
            if (!evaluateThreshold(currentRam, conditions.ram_threshold, "RAM")) {
                return false;
            }
        }
    }
    
    // Check load average condition
    if (!conditions.loadavg_threshold.empty()) {
        float currentLoad = getCurrentLoadAverage();
        if (currentLoad < 0) {
            std::cerr << "Warning: Could not read load average, ignoring load condition" << std::endl;
        } else {
            if (!evaluateThreshold(currentLoad, conditions.loadavg_threshold, "Load")) {
                return false;
            }
        }
    }
    
    // Check disk usage conditions
    for (const auto& [path, threshold] : conditions.disk_thresholds) {
        float currentDisk = getCurrentDiskUsage(path);
        if (currentDisk < 0) {
            std::cerr << "Warning: Could not read disk usage for " << path 
                      << ", ignoring disk condition" << std::endl;
            continue;
        }
        
        if (!evaluateThreshold(currentDisk, threshold, "Disk[" + path + "]")) {
            return false;
        }
    }
    
    return true;
}

/**
 * Get current CPU usage percentage (0-100)
 * @return CPU usage percentage, -1 on error
 */
float JobConfig::getCurrentCpuUsage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return -1;
    }
    
    std::string line;
    std::getline(file, line);
    file.close();
    
    // Parse first line: "cpu user nice system idle iowait irq softirq steal guest guest_nice"
    std::istringstream iss(line);
    std::string cpu_label;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    
    if (!(iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
        return -1;
    }
    
    // Calculate total and idle time
    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    long idle_time = idle + iowait;
    
    // For a more accurate reading, we should compare with previous values
    // For simplicity, we'll use a single snapshot approach
    // In production, consider maintaining previous values for better accuracy
    
    static long prev_total = 0;
    static long prev_idle = 0;
    
    if (prev_total == 0) {
        // First reading, store values and return 0
        prev_total = total;
        prev_idle = idle_time;
        return 0.0f;
    }
    
    long total_diff = total - prev_total;
    long idle_diff = idle_time - prev_idle;
    
    prev_total = total;
    prev_idle = idle_time;
    
    if (total_diff == 0) {
        return 0.0f;
    }
    
    float cpu_usage = ((float)(total_diff - idle_diff) / total_diff) * 100.0f;
    return std::max(0.0f, std::min(100.0f, cpu_usage));
}

/**
 * Get current RAM usage percentage (0-100)
 * @return RAM usage percentage, -1 on error
 */
float JobConfig::getCurrentRamUsage() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return -1;
    }
    
    long memTotal = 0, memFree = 0, buffers = 0, cached = 0;
    std::string line;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        long value;
        std::string unit;
        
        if (iss >> key >> value >> unit) {
            if (key == "MemTotal:") {
                memTotal = value;
            } else if (key == "MemFree:") {
                memFree = value;
            } else if (key == "Buffers:") {
                buffers = value;
            } else if (key == "Cached:") {
                cached = value;
            }
        }
        
        // Early exit if we have all needed values
        if (memTotal > 0 && memFree >= 0 && buffers >= 0 && cached >= 0) {
            break;
        }
    }
    
    file.close();
    
    if (memTotal <= 0) {
        return -1;
    }
    
    // Calculate used memory (excluding buffers and cache)
    long memUsed = memTotal - memFree - buffers - cached;
    float usage = ((float)memUsed / memTotal) * 100.0f;
    
    return std::max(0.0f, std::min(100.0f, usage));
}

/**
 * Get current 1-minute load average
 * @return Load average, -1 on error
 */
float JobConfig::getCurrentLoadAverage() {
    std::ifstream file("/proc/loadavg");
    if (!file.is_open()) {
        return -1;
    }
    
    float loadavg1, loadavg5, loadavg15;
    if (!(file >> loadavg1 >> loadavg5 >> loadavg15)) {
        file.close();
        return -1;
    }
    
    file.close();
    return loadavg1;
}

/**
 * Get current disk usage for specified path
 * @param path Filesystem path to check
 * @return Disk usage percentage (0-100), -1 on error
 */
float JobConfig::getCurrentDiskUsage(const std::string& path) {
    // Use statvfs system call for accurate disk usage
    struct statvfs stat;
    
    if (statvfs(path.c_str(), &stat) != 0) {
        return -1;
    }
    
    // Calculate usage percentage
    unsigned long long total = stat.f_blocks * stat.f_frsize;
    unsigned long long free = stat.f_bavail * stat.f_frsize;
    unsigned long long used = total - free;
    
    if (total == 0) {
        return -1;
    }
    
    float usage = ((float)used / total) * 100.0f;
    return std::max(0.0f, std::min(100.0f, usage));
}

/**
 * Evaluate threshold condition against current value
 * @param currentValue Current system metric value
 * @param threshold Threshold string (e.g., "<80%", ">90%")
 * @param metricName Name of metric for logging
 * @return true if condition is met
 */
bool JobConfig::evaluateThreshold(float currentValue, const std::string& threshold, const std::string& metricName) {
    if (threshold.empty()) {
        return true;
    }
    
    // Parse threshold format: "<80%" or ">50%"
    char operator_char = threshold[0];
    if (operator_char != '<' && operator_char != '>') {
        std::cerr << "Warning: Invalid " << metricName << " threshold format: " << threshold << std::endl;
        return true; // Allow execution on invalid format
    }
    
    // Extract numeric value
    std::string value_str = threshold.substr(1);
    if (value_str.back() == '%') {
        value_str.pop_back();
    }
    
    float threshold_value;
    try {
        threshold_value = std::stof(value_str);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Invalid " << metricName << " threshold value: " << threshold << std::endl;
        return true; // Allow execution on parsing error
    }
    
    // Evaluate condition
    bool condition_met;
    if (operator_char == '<') {
        condition_met = currentValue < threshold_value;
    } else { // operator_char == '>'
        condition_met = currentValue > threshold_value;
    }
    
    // Log condition evaluation for debugging
    if (!condition_met) {
        std::cout << "Job condition not met: " << metricName << " " 
                  << currentValue << "% " << threshold << std::endl;
    }
    
    return condition_met;
}