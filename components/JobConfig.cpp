/**
 * @file JobConfig.cpp
 * @brief JSON-based job configuration management, schedule resolution,
 *        and runtime system-resource condition checks.
 */

#include "JobConfig.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// ---------------------------------------------------------------------------
// Loading / parsing
// ---------------------------------------------------------------------------

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

std::vector<CronJob> JobConfig::parseJobsFromJson(const std::string& json_string) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_string);
        return parseJobsFromJson(std::move(j));
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return {};
    }
}

std::vector<CronJob> JobConfig::parseJobsFromJson(nlohmann::json&& j) {
    std::vector<CronJob> jobs;

    if (!j.contains("jobs") || !j["jobs"].is_array()) {
        std::cerr << "Error: JSON must contain 'jobs' array" << std::endl;
        return {};
    }

    try {
        for (const auto& job_json : j["jobs"]) {
            CronJob job{};

            // Required fields - skip the job entirely if missing.
            if (!job_json.contains("description") || !job_json.contains("command")) {
                std::cerr << "Warning: Skipping job missing required fields (description, command)" << std::endl;
                continue;
            }

            job.description = job_json["description"].get<std::string>();
            job.command = job_json["command"].get<std::string>();

            // Schedule: accepts either a cron string ("0 9 * * 1-5") or an object.
            if (job_json.contains("schedule")) {
                const auto& sched = job_json["schedule"];

                if (sched.is_string()) {
                    parseScheduleFromString(job, sched.get<std::string>());
                } else if (sched.is_object()) {
                    job.schedule.minute = sched.value("minute", "*");
                    job.schedule.hour = sched.value("hour", "*");
                    job.schedule.day_of_month = sched.value("day_of_month", "*");
                    job.schedule.month = sched.value("month", "*");
                    job.schedule.day_of_week = sched.value("day_of_week", "*");
                }

                parseScheduleToLegacyFormat(job);
            } else {
                // Legacy flat format fallback: {"hour": 3, "minute": 0, ...}
                job.hour = job_json.value("hour", -1);
                job.minute = job_json.value("minute", -1);
                job.minute_interval = 0;
                job.hour_interval = 0;
                job.day_param = job_json.value("day", -1);
                job.month_param = job_json.value("month", -1);
                job.frequency = CronFrequency::DAILY;

                convertLegacyToSchedule(job);
            }

            // Conditions (optional). Field names match the documented
            // jobs.json format: "cpu", "ram", "loadavg", "disk".
            if (job_json.contains("conditions")) {
                const auto& cond = job_json["conditions"];
                job.conditions.cpu_threshold = cond.value("cpu", "");
                job.conditions.ram_threshold = cond.value("ram", "");
                job.conditions.loadavg_threshold = cond.value("loadavg", "");

                if (cond.contains("disk") && cond["disk"].is_object()) {
                    for (const auto& [path, threshold] : cond["disk"].items()) {
                        job.conditions.disk_thresholds[path] = threshold.get<std::string>();
                    }
                }
            }

            // NOTE: system-resource conditions are intentionally NOT checked
            // here. They must be evaluated at execution time (see
            // JobConfig::checkJobConditions), right before a job is about to
            // run - not at config-load time. Otherwise a job would be
            // permanently dropped from the schedule just because the system
            // happened to be busy at the moment the config file was read.
            jobs.push_back(std::move(job));
        }
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing job configuration: " << e.what() << std::endl;
        return {};
    }

    return jobs;
}

bool JobConfig::saveJobsToJson(const std::vector<CronJob>& jobs, const std::string& filename) {
    try {
        nlohmann::json config;
        config["jobs"] = nlohmann::json::array();

        for (const auto& job : jobs) {
            nlohmann::json job_json;
            job_json["description"] = job.description;
            job_json["command"] = job.command;

            nlohmann::json schedule_json;
            schedule_json["minute"] = job.schedule.minute;
            schedule_json["hour"] = job.schedule.hour;
            schedule_json["day_of_month"] = job.schedule.day_of_month;
            schedule_json["month"] = job.schedule.month;
            schedule_json["day_of_week"] = job.schedule.day_of_week;
            job_json["schedule"] = schedule_json;

            const bool hasConditions = !job.conditions.cpu_threshold.empty() ||
                                        !job.conditions.ram_threshold.empty() ||
                                        !job.conditions.loadavg_threshold.empty() ||
                                        !job.conditions.disk_thresholds.empty();

            if (hasConditions) {
                nlohmann::json conditions_json;
                if (!job.conditions.cpu_threshold.empty())
                    conditions_json["cpu"] = job.conditions.cpu_threshold;
                if (!job.conditions.ram_threshold.empty())
                    conditions_json["ram"] = job.conditions.ram_threshold;
                if (!job.conditions.loadavg_threshold.empty())
                    conditions_json["loadavg"] = job.conditions.loadavg_threshold;

                if (!job.conditions.disk_thresholds.empty()) {
                    nlohmann::json disk_json;
                    for (const auto& [path, threshold] : job.conditions.disk_thresholds) {
                        disk_json[path] = threshold;
                    }
                    conditions_json["disk"] = disk_json;
                }

                job_json["conditions"] = conditions_json;
            }

            config["jobs"].push_back(job_json);
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot create file: " << filename << std::endl;
            return false;
        }

        file << config.dump(2);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error saving jobs to " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

bool JobConfig::validateJobsFile(const std::string& filename, std::string& errorMsg) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        errorMsg = "Cannot open file: " + filename;
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try {
        nlohmann::json j = nlohmann::json::parse(content);

        if (!j.contains("jobs") || !j["jobs"].is_array()) {
            errorMsg = "JSON must contain 'jobs' array";
            return false;
        }

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

bool JobConfig::isValidJobsJson(const std::string& json_string) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_string);
        return j.contains("jobs") && j["jobs"].is_array();
    } catch (const nlohmann::json::parse_error&) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Schedule parsing
// ---------------------------------------------------------------------------

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

bool JobConfig::parseIntervalField(const std::string& field, int& value, int& interval) {
    if (field == "*") {
        value = -1;
        interval = 0;
        return true;
    }

    // "*/N" interval form (e.g. "*/15" -> every 15 units)
    if (field.size() > 2 && field[0] == '*' && field[1] == '/') {
        try {
            interval = std::stoi(field.substr(2));
        } catch (const std::exception&) {
            return false;
        }
        if (interval <= 0) {
            return false;
        }
        value = -2;
        return true;
    }

    // Fixed numeric value
    try {
        value = std::stoi(field);
        interval = 0;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void JobConfig::parseScheduleToLegacyFormat(CronJob& job) {
    bool ok = true;

    ok &= parseIntervalField(job.schedule.minute, job.minute, job.minute_interval);
    ok &= parseIntervalField(job.schedule.hour, job.hour, job.hour_interval);

    // day_of_month / month don't support "*/N" in this version - only "*" or a fixed value.
    try {
        job.day_param = (job.schedule.day_of_month == "*") ? -1 : std::stoi(job.schedule.day_of_month);
        job.month_param = (job.schedule.month == "*") ? -1 : std::stoi(job.schedule.month);
    } catch (const std::exception&) {
        ok = false;
    }

    if (!ok) {
        std::cerr << "Warning: Could not parse schedule for job '" << job.description
                  << "' - defaulting to DAILY at any time. Check the schedule fields." << std::endl;
        job.minute = -1;
        job.hour = -1;
        job.minute_interval = 0;
        job.hour_interval = 0;
        job.day_param = -1;
        job.month_param = -1;
        job.frequency = CronFrequency::DAILY;
        return;
    }

    // Determine frequency from day_of_week / day_of_month / month.
    if (job.schedule.day_of_week == "1-5") {
        job.frequency = CronFrequency::WEEKDAY;
    } else if (job.schedule.day_of_week == "0,6" || job.schedule.day_of_week == "6,0") {
        job.frequency = CronFrequency::WEEKEND;
    } else if (job.schedule.day_of_week != "*") {
        job.frequency = CronFrequency::WEEKLY;
        // For WEEKLY frequency, day_param must hold the weekday number (0-6),
        // not the day-of-month, since CronEngine matches it against tm_wday.
        try {
            job.day_param = std::stoi(job.schedule.day_of_week);
        } catch (const std::exception&) {
            std::cerr << "Warning: Invalid day_of_week '" << job.schedule.day_of_week
                      << "' for job '" << job.description << "', defaulting to Sunday (0)" << std::endl;
            job.day_param = 0;
        }
    } else if (job.schedule.day_of_month != "*") {
        job.frequency = CronFrequency::MONTHLY;
    } else if (job.schedule.month != "*") {
        job.frequency = CronFrequency::YEARLY;
    } else {
        job.frequency = CronFrequency::DAILY;
    }
}

void JobConfig::convertLegacyToSchedule(CronJob& job) {
    job.schedule.minute = (job.minute == -1) ? "*" : std::to_string(job.minute);
    job.schedule.hour = (job.hour == -1) ? "*" : std::to_string(job.hour);
    job.schedule.day_of_month = (job.day_param == -1) ? "*" : std::to_string(job.day_param);
    job.schedule.month = (job.month_param == -1) ? "*" : std::to_string(job.month_param);

    switch (job.frequency) {
        case CronFrequency::WEEKDAY:
            job.schedule.day_of_week = "1-5";
            break;
        case CronFrequency::WEEKEND:
            job.schedule.day_of_week = "0,6";
            break;
        default:
            job.schedule.day_of_week = "*";
            break;
    }
}

// ---------------------------------------------------------------------------
// Runtime system-resource condition checks
// ---------------------------------------------------------------------------

bool JobConfig::checkJobConditions(const JobConditions& conditions) {
    if (conditions.cpu_threshold.empty() &&
        conditions.ram_threshold.empty() &&
        conditions.loadavg_threshold.empty() &&
        conditions.disk_thresholds.empty()) {
        return true; // No conditions configured - always allowed to run.
    }

    if (!conditions.cpu_threshold.empty()) {
        float currentCpu = getCurrentCpuUsage();
        if (currentCpu < 0) {
            std::cerr << "Warning: Could not read CPU usage, ignoring CPU condition" << std::endl;
        } else if (!evaluateThreshold(currentCpu, conditions.cpu_threshold, "CPU")) {
            return false;
        }
    }

    if (!conditions.ram_threshold.empty()) {
        float currentRam = getCurrentRamUsage();
        if (currentRam < 0) {
            std::cerr << "Warning: Could not read RAM usage, ignoring RAM condition" << std::endl;
        } else if (!evaluateThreshold(currentRam, conditions.ram_threshold, "RAM")) {
            return false;
        }
    }

    if (!conditions.loadavg_threshold.empty()) {
        float currentLoad = getCurrentLoadAverage();
        if (currentLoad < 0) {
            std::cerr << "Warning: Could not read load average, ignoring load condition" << std::endl;
        } else if (!evaluateThreshold(currentLoad, conditions.loadavg_threshold, "Load")) {
            return false;
        }
    }

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

float JobConfig::getCurrentCpuUsage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return -1;
    }

    std::string line;
    std::getline(file, line);

    // First line format: "cpu user nice system idle iowait irq softirq steal ..."
    std::istringstream iss(line);
    std::string cpu_label;
    long user, nice, system, idle, iowait, irq, softirq, steal;

    if (!(iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
        return -1;
    }

    long total = user + nice + system + idle + iowait + irq + softirq + steal;
    long idle_time = idle + iowait;

    // CPU usage requires two samples over time (it's a rate, not an
    // instantaneous value). We keep the previous sample between calls:
    // the very first call after startup has nothing to compare against,
    // so it reports 0% for that one call only.
    static long prev_total = 0;
    static long prev_idle = 0;

    if (prev_total == 0) {
        prev_total = total;
        prev_idle = idle_time;
        return 0.0f;
    }

    long total_diff = total - prev_total;
    long idle_diff = idle_time - prev_idle;

    prev_total = total;
    prev_idle = idle_time;

    if (total_diff <= 0) {
        return 0.0f;
    }

    float cpu_usage = ((float)(total_diff - idle_diff) / total_diff) * 100.0f;
    return std::max(0.0f, std::min(100.0f, cpu_usage));
}

float JobConfig::getCurrentRamUsage() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return -1;
    }

    // Sentinel -1 = "not read yet", so we never confuse a genuinely-missing
    // field with a value of 0 (which is what caused the previous version to
    // always report ~100% usage: it exited the loop right after MemTotal,
    // before MemFree/Buffers/Cached were ever parsed).
    long memTotal = -1, memFree = -1, buffers = -1, cached = -1;
    std::string key;
    long value;
    std::string unit;

    while (file >> key >> value >> unit) {
        if (key == "MemTotal:") memTotal = value;
        else if (key == "MemFree:") memFree = value;
        else if (key == "Buffers:") buffers = value;
        else if (key == "Cached:") cached = value;

        if (memTotal >= 0 && memFree >= 0 && buffers >= 0 && cached >= 0) {
            break; // All fields found, no need to read the rest of the file.
        }
    }

    if (memTotal <= 0 || memFree < 0) {
        return -1; // Essential fields missing - can't compute usage.
    }

    // Buffers/Cached may be absent on some kernels; treat as 0 if so.
    if (buffers < 0) buffers = 0;
    if (cached < 0) cached = 0;

    long memUsed = memTotal - memFree - buffers - cached;
    float usage = ((float)memUsed / memTotal) * 100.0f;
    return std::max(0.0f, std::min(100.0f, usage));
}

float JobConfig::getCurrentLoadAverage() {
    std::ifstream file("/proc/loadavg");
    if (!file.is_open()) {
        return -1;
    }

    float loadavg1, loadavg5, loadavg15;
    if (!(file >> loadavg1 >> loadavg5 >> loadavg15)) {
        return -1;
    }

    return loadavg1;
}

float JobConfig::getCurrentDiskUsage(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        return -1;
    }

    unsigned long long total = (unsigned long long)stat.f_blocks * stat.f_frsize;
    if (total == 0) {
        return -1;
    }

    unsigned long long free = (unsigned long long)stat.f_bavail * stat.f_frsize;
    unsigned long long used = total - free;

    float usage = ((float)used / (float)total) * 100.0f;
    return std::max(0.0f, std::min(100.0f, usage));
}

bool JobConfig::evaluateThreshold(float currentValue, const std::string& threshold, const std::string& metricName) {
    if (threshold.empty()) {
        return true;
    }

    char operator_char = threshold[0];
    if (operator_char != '<' && operator_char != '>') {
        std::cerr << "Warning: Invalid " << metricName << " threshold format: " << threshold << std::endl;
        return true; // Allow execution on invalid format rather than blocking jobs forever.
    }

    std::string value_str = threshold.substr(1);
    if (!value_str.empty() && value_str.back() == '%') {
        value_str.pop_back();
    }

    if (value_str.empty()) {
        std::cerr << "Warning: Invalid " << metricName << " threshold value: " << threshold << std::endl;
        return true;
    }

    float threshold_value;
    try {
        threshold_value = std::stof(value_str);
    } catch (const std::exception&) {
        std::cerr << "Warning: Invalid " << metricName << " threshold value: " << threshold << std::endl;
        return true;
    }

    bool condition_met = (operator_char == '<') ? (currentValue < threshold_value)
                                                  : (currentValue > threshold_value);

    if (!condition_met) {
        std::cout << "Job condition not met: " << metricName << " "
                  << currentValue << "% " << threshold << std::endl;
    }

    return condition_met;
}