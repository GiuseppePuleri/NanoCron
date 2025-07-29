#include "JobConfig.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <map>

using json = nlohmann::json;

std::vector<CronJob> JobConfig::loadJobs(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Cannot open " << filename << ". Creating example config..." << std::endl;
        createExampleConfig(filename);
        return getFallbackJobs();
    }
    
    try {
        json j;
        file >> j;
        file.close();
        
        return parseJobsFromJson(j.dump());
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        std::cerr << "Using fallback configuration..." << std::endl;
        file.close();
        return getFallbackJobs();
    }
}

std::vector<CronJob> JobConfig::parseJobsFromJson(const std::string& json_string) {
    std::vector<CronJob> jobs;
    
    try {
        json j = json::parse(json_string);
        
        // Check if "jobs" array exists
        if (!j.contains("jobs") || !j["jobs"].is_array()) {
            throw std::runtime_error("JSON must contain 'jobs' array");
        }
        
        // Parse each job
        for (const auto& job_json : j["jobs"]) {
            CronJob job;
            
            // Required fields
            if (job_json.contains("description") && job_json["description"].is_string()) {
                job.description = job_json["description"];
            } else {
                throw std::runtime_error("Job missing required 'description' field");
            }
            
            if (job_json.contains("command") && job_json["command"].is_string()) {
                job.command = job_json["command"];
            } else {
                throw std::runtime_error("Job missing required 'command' field");
            }
            
            // Parse schedule
            if (job_json.contains("schedule") && job_json["schedule"].is_object()) {
                const auto& schedule = job_json["schedule"];
                
                job.schedule.minute = schedule.value("minute", "*");
                job.schedule.hour = schedule.value("hour", "*");
                job.schedule.day_of_month = schedule.value("day_of_month", "*");
                job.schedule.month = schedule.value("month", "*");
                job.schedule.day_of_week = schedule.value("day_of_week", "*");
            } else {
                throw std::runtime_error("Job missing required 'schedule' object");
            }
            
            // Parse conditions (optional)
            if (job_json.contains("conditions") && job_json["conditions"].is_object()) {
                const auto& conditions = job_json["conditions"];
                
                job.conditions.cpu_threshold = conditions.value("cpu", "");
                job.conditions.ram_threshold = conditions.value("ram", "");
                job.conditions.loadavg_threshold = conditions.value("loadavg", "");
                
                // Parse disk conditions
                if (conditions.contains("disk") && conditions["disk"].is_object()) {
                    for (const auto& [path, threshold] : conditions["disk"].items()) {
                        job.conditions.disk_thresholds[path] = threshold;
                    }
                }
            }
            
            // Convert to legacy format for compatibility
            parseScheduleToLegacyFormat(job);
            
            jobs.push_back(job);
            
            std::cout << "Loaded job: " << job.description << " [" << job.command << "]" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return getFallbackJobs();
    }
    
    if (jobs.empty()) {
        std::cerr << "No valid jobs found in JSON. Using fallback..." << std::endl;
        return getFallbackJobs();
    }
    
    return jobs;
}

void JobConfig::parseScheduleToLegacyFormat(CronJob& job) {
    // Convert JSON schedule to legacy format for existing engine compatibility
    
    // Parse minute
    if (job.schedule.minute == "*") {
        job.minute = -1; // Special value for "any minute"
    } else if (job.schedule.minute.find("*/") == 0) {
        // Handle */X format (every X minutes)
        job.minute = -2; // Special value for interval
        // TODO: Store interval value for proper handling
    } else {
        try {
            job.minute = std::stoi(job.schedule.minute);
        } catch (const std::exception&) {
            job.minute = 0; // Default fallback
        }
    }
    
    // Parse hour
    if (job.schedule.hour == "*") {
        job.hour = -1; // Special value for "any hour"
    } else {
        try {
            job.hour = std::stoi(job.schedule.hour);
        } catch (const std::exception&) {
            job.hour = 0; // Default fallback
        }
    }
    
    // Determine frequency based on schedule
    if (job.schedule.day_of_week != "*") {
        job.frequency = CronFrequency::WEEKLY;
        try {
            job.day_param = std::stoi(job.schedule.day_of_week);
        } catch (const std::exception&) {
            job.day_param = 0;
        }
    } else if (job.schedule.day_of_month != "*") {
        job.frequency = CronFrequency::MONTHLY;
        try {
            job.day_param = std::stoi(job.schedule.day_of_month);
        } catch (const std::exception&) {
            job.day_param = 1;
        }
    } else if (job.schedule.month != "*") {
        job.frequency = CronFrequency::YEARLY;
        try {
            job.day_param = std::stoi(job.schedule.day_of_month);
            job.month_param = std::stoi(job.schedule.month);
        } catch (const std::exception&) {
            job.day_param = 1;
            job.month_param = 1;
        }
    } else {
        job.frequency = CronFrequency::DAILY;
        job.day_param = 0;
        job.month_param = 0;
    }
}

bool JobConfig::checkJobConditions(const JobConditions& conditions) {
    // TODO: Implement system condition checking
    // This would check CPU, RAM, disk usage, etc.
    // For now, return true (always execute)
    
    // Example implementation outline:
    // - Parse threshold strings (e.g., ">90%", "<95%")
    // - Get current system stats
    // - Compare with thresholds
    // - Return false if conditions not met
    
    return true;
}

void JobConfig::createExampleConfig(const std::string& filename) {
    json config;
    
    // Create jobs array
    config["jobs"] = json::array();
    
    // Job 1: Daily session cleanup
    json job1;
    job1["description"] = "Daily session cleanup";
    job1["schedule"]["minute"] = "0";
    job1["schedule"]["hour"] = "23";
    job1["schedule"]["day_of_month"] = "*";
    job1["schedule"]["month"] = "*";
    job1["schedule"]["day_of_week"] = "*";
    job1["command"] = "/var/www/app/Cpp/MyCron/Jobs/closeSessionJob";
    job1["conditions"]["cpu"] = "<95%";
    job1["conditions"]["ram"] = "<90%";
    job1["conditions"]["loadavg"] = "<10";
    config["jobs"].push_back(job1);
    
    // Job 2: Monthly XML generation
    json job2;
    job2["description"] = "Monthly XML generation";
    job2["schedule"]["minute"] = "0";
    job2["schedule"]["hour"] = "5";
    job2["schedule"]["day_of_month"] = "1";
    job2["schedule"]["month"] = "*";
    job2["schedule"]["day_of_week"] = "*";
    job2["command"] = "/var/www/app/Cpp/MyCron/Jobs/makeAttendanceJob";
    job2["conditions"]["disk"]["/var"] = "<95%";
    config["jobs"].push_back(job2);
    
    // Job 3: Monthly PDF generation
    json job3;
    job3["description"] = "Monthly PDF generation";
    job3["schedule"]["minute"] = "0";
    job3["schedule"]["hour"] = "1";
    job3["schedule"]["day_of_month"] = "1";
    job3["schedule"]["month"] = "*";
    job3["schedule"]["day_of_week"] = "*";
    job3["command"] = "/var/www/app/Cpp/MyCron/Jobs/makeReportJob";
    job3["conditions"] = json::object(); // Empty conditions
    config["jobs"].push_back(job3);
    
    // Job 4: CPU usage check every 5 minutes
    json job4;
    job4["description"] = "CPU usage check every 5 minutes";
    job4["schedule"]["minute"] = "*/5";
    job4["schedule"]["hour"] = "*";
    job4["schedule"]["day_of_month"] = "*";
    job4["schedule"]["month"] = "*";
    job4["schedule"]["day_of_week"] = "*";
    job4["command"] = "/usr/local/bin/check_cpu.sh --threshold 90";
    job4["conditions"]["cpu"] = ">90%";
    job4["conditions"]["ram"] = ">80%";
    job4["conditions"]["loadavg"] = ">5";
    job4["conditions"]["disk"]["/var"] = ">95%";
    config["jobs"].push_back(job4);
    
    // Write to file
    std::ofstream file(filename);
    if (file.is_open()) {
        file << config.dump(2); // Pretty print with 2-space indentation
        file.close();
        std::cout << "Created example configuration file: " << filename << std::endl;
    } else {
        std::cerr << "Error: Cannot create example config file: " << filename << std::endl;
    }
}

std::vector<CronJob> JobConfig::getFallbackJobs() {
    std::vector<CronJob> jobs;
    
    // Fallback to hardcoded jobs if JSON fails
    CronJob job1;
    job1.description = "Daily session cleanup";
    job1.hour = 23;
    job1.minute = 0;
    job1.frequency = CronFrequency::DAILY;
    job1.day_param = 0;
    job1.month_param = 0;
    job1.command = "/var/www/app/Cpp/MyCron/Jobs/closeSessionJob";
    jobs.push_back(job1);
    
    CronJob job2;
    job2.description = "Monthly XML generation";
    job2.hour = 5;
    job2.minute = 0;
    job2.frequency = CronFrequency::MONTHLY;
    job2.day_param = 1;
    job2.month_param = 0;
    job2.command = "/var/www/app/Cpp/MyCron/Jobs/makeAttendanceJob";
    jobs.push_back(job2);
    
    CronJob job3;
    job3.description = "Monthly PDF generation";
    job3.hour = 1;
    job3.minute = 0;
    job3.frequency = CronFrequency::MONTHLY;
    job3.day_param = 1;
    job3.month_param = 0;
    job3.command = "/var/www/app/Cpp/MyCron/Jobs/makeReportJob";
    jobs.push_back(job3);
    
    return jobs;
}

bool JobConfig::saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename) {
    try {
        json config;
        config["jobs"] = json::array();
        
        for (const CronJob& job : jobs) {
            json job_json;
            job_json["description"] = job.description;
            job_json["command"] = job.command;
            
            // Schedule
            job_json["schedule"]["minute"] = job.schedule.minute;
            job_json["schedule"]["hour"] = job.schedule.hour;
            job_json["schedule"]["day_of_month"] = job.schedule.day_of_month;
            job_json["schedule"]["month"] = job.schedule.month;
            job_json["schedule"]["day_of_week"] = job.schedule.day_of_week;
            
            // Conditions
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
            
            config["jobs"].push_back(job_json);
        }
        
        std::ofstream file(filename);
        if (file.is_open()) {
            file << config.dump(2);
            file.close();
            return true;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving JSON: " << e.what() << std::endl;
    }
    
    return false;
}