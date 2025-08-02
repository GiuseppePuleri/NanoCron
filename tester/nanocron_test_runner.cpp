#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <atomic>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <cstring>

// Include dei componenti nanoCron per il parsing
#include "../components/json.hpp"
#include "../components/CronTypes.h"

using json = nlohmann::json;

/**
 * Classe per misurare le performance di nanoCron parsing (UNIFIED VERSION)
 */
class PerformanceMetrics {
private:
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
    size_t initial_memory;
    size_t peak_memory;
    std::atomic<bool> monitoring;
    std::thread cpu_monitor_thread;
    double peak_cpu_usage;
    double avg_cpu_usage;
    std::vector<double> cpu_samples;
    std::string test_name;
    std::string log_directory;
    
    // Metadata di validità del test (come system cron)
    struct TestValidityMetrics {
        int json_jobs_parsed;
        int json_objects_processed;
        bool successful_parsing;
        double test_duration_ms;
        std::string parsing_method;
        size_t json_file_size_bytes;
    } validity_metrics;
    
    bool createLogDirectory(const std::string& path) {
        struct stat st = {0};
        if (stat(path.c_str(), &st) == -1) {
            if (mkdir(path.c_str(), 0755) == -1) {
                std::cerr << "Warning: Could not create directory " << path << std::endl;
                return false;
            }
        }
        return true;
    }
    
    std::string normalizePath(const std::string& dir, const std::string& filename) {
        std::string normalized_dir = dir;
        if (!normalized_dir.empty() && normalized_dir.back() == '/') {
            normalized_dir.pop_back();
        }
        return normalized_dir + "/" + filename;
    }
    
    // UNIFICATA: Usa /proc/self/status come system cron
    size_t getCurrentMemoryUsage() {
        std::ifstream status("/proc/self/status");
        std::string line;
        
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                size_t pos = line.find_first_of("0123456789");
                if (pos != std::string::npos) {
                    return std::stoul(line.substr(pos));
                }
            }
        }
        return 0;
    }
    
    // CPU usage del processo corrente (già corretto)
    double getCurrentCPUUsage() {
        static clock_t prev_cpu = 0;
        static auto prev_time = std::chrono::steady_clock::now();
        
        clock_t current_cpu = clock();
        auto current_time = std::chrono::steady_clock::now();
        
        if (prev_cpu == 0) {
            prev_cpu = current_cpu;
            prev_time = current_time;
            return 0.0;
        }
        
        auto time_diff = std::chrono::duration_cast<std::chrono::microseconds>(current_time - prev_time);
        clock_t cpu_diff = current_cpu - prev_cpu;
        
        prev_cpu = current_cpu;
        prev_time = current_time;
        
        if (time_diff.count() == 0) return 0.0;
        
        double cpu_percentage = (100.0 * cpu_diff * 1000000) / (CLOCKS_PER_SEC * time_diff.count());
        return std::min(cpu_percentage, 100.0);
    }
    
    void monitorProcess() {
        while (monitoring) {
            double cpu = getCurrentCPUUsage();
            size_t memory = getCurrentMemoryUsage();
            
            cpu_samples.push_back(cpu);
            if (cpu > peak_cpu_usage) {
                peak_cpu_usage = cpu;
            }
            
            if (memory > peak_memory) {
                peak_memory = memory;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
public:
    PerformanceMetrics(const std::string& name, const std::string& log_dir = "./test_logs") : 
        test_name(name), peak_memory(0), monitoring(false), peak_cpu_usage(0.0), 
        avg_cpu_usage(0.0), log_directory(log_dir) {
        
        createLogDirectory(log_directory);
        
        // Inizializza metrics di validità
        validity_metrics = {0, 0, false, 0.0, "Optimized JSON Parse", 0};
        
        std::cout << "PerformanceMetrics (OPTIMIZED) initialized for " << name << " with log directory: " << log_directory << std::endl;
    }
    
    ~PerformanceMetrics() {
        if (monitoring) {
            stopMeasuring();
        }
    }
    
    void startMeasuring() {
        initial_memory = getCurrentMemoryUsage();
        peak_memory = initial_memory;
        peak_cpu_usage = 0.0;
        cpu_samples.clear();
        
        monitoring = true;
        cpu_monitor_thread = std::thread(&PerformanceMetrics::monitorProcess, this);
        
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    void stopMeasuring() {
        end_time = std::chrono::high_resolution_clock::now();
        
        monitoring = false;
        if (cpu_monitor_thread.joinable()) {
            cpu_monitor_thread.join();
        }
        
        // Calcola CPU media
        if (!cpu_samples.empty()) {
            double sum = 0.0;
            for (double sample : cpu_samples) {
                sum += sample;
            }
            avg_cpu_usage = sum / cpu_samples.size();
        }
        
        // Calcola durata test per validità
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        validity_metrics.test_duration_ms = duration.count() / 1000.0;
    }
    
    double getParseTimeMs() const {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        return duration.count() / 1000.0;
    }
    
    size_t getMemoryUsed() const {
        return peak_memory > initial_memory ? peak_memory - initial_memory : 0;
    }
    
    size_t getAbsolutePeakMemory() const { return peak_memory; }
    size_t getAbsoluteInitialMemory() const { return initial_memory; }
    double getPeakCPU() const { return peak_cpu_usage; }
    double getAvgCPU() const { return avg_cpu_usage; }
    
    // NUOVO: Imposta metadata di validità
    void setValidityMetrics(int jobs_parsed, int objects_processed, bool success, size_t file_size) {
        validity_metrics.json_jobs_parsed = jobs_parsed;
        validity_metrics.json_objects_processed = objects_processed;
        validity_metrics.successful_parsing = success;
        validity_metrics.json_file_size_bytes = file_size;
    }
    
    void logMetrics(const std::string& operation = "Parse") {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::string final_log_path = normalizePath(log_directory, "performance.log");
        
        std::cout << "Metrics logged to " << final_log_path << std::endl;
        
        std::ofstream log(final_log_path, std::ios::app);
        if (log.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            log << "=== " << test_name << " " << operation << " Metrics (OPTIMIZED) ===" << std::endl;
            log << "Timestamp: " << std::ctime(&time_t);
            log << "Parse Time: " << duration.count() << " microseconds" << std::endl;
            log << "Parse Time (ms): " << duration.count() / 1000.0 << " ms" << std::endl;
            log << "Initial Memory: " << initial_memory << " KB" << std::endl;
            log << "Peak Memory: " << peak_memory << " KB" << std::endl;
            log << "Memory Used: " << getMemoryUsed() << " KB" << std::endl;
            log << "Peak CPU Usage: " << peak_cpu_usage << "%" << std::endl;
            log << "Average CPU Usage: " << avg_cpu_usage << "%" << std::endl;
            log << "CPU Samples: " << cpu_samples.size() << std::endl;
            
            // NUOVO: Metadata di validità (come system cron)
            log << "--- Test Validity Metrics ---" << std::endl;
            log << "Parsing Method: " << validity_metrics.parsing_method << std::endl;
            log << "JSON Jobs Parsed: " << validity_metrics.json_jobs_parsed << std::endl;
            log << "JSON Objects Processed: " << validity_metrics.json_objects_processed << std::endl;
            log << "Successful Parsing: " << (validity_metrics.successful_parsing ? "YES" : "NO") << std::endl;
            log << "JSON File Size: " << validity_metrics.json_file_size_bytes << " bytes" << std::endl;
            log << "Test Duration: " << validity_metrics.test_duration_ms << " ms" << std::endl;
            log << "Memory Measurement: /proc/self/status (OPTIMIZED)" << std::endl;
            log << "CPU Measurement: clock() current process (OPTIMIZED)" << std::endl;
            log << "Optimizations Applied: Move Semantics, Pre-allocation, Direct JSON Parsing" << std::endl;
            log << "----------------------------------------" << std::endl;
            log.close();
        } else {
            std::cerr << "Error: Could not open log file: " << final_log_path << std::endl;
        }
    }
};

/**
 * NUOVO: TestJobConfig aggiornato con le stesse ottimizzazioni di JobConfig.cpp
 */
class TestJobConfig {
public:
    // OTTIMIZZAZIONE: Caricamento diretto con move semantics come in JobConfig.cpp
    static std::vector<CronJob> loadJobs(const std::string& filename, size_t& file_size, int& objects_processed) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open " << filename << std::endl;
            file_size = 0;
            objects_processed = 0;
            return {};
        }
        
        // Calcola dimensione file
        file.seekg(0, std::ios::end);
        file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        try {
            json j;
            file >> j;
            file.close();
            
            // OTTIMIZZAZIONE: Parsing diretto senza conversione a string
            return parseJobsFromJson(std::move(j), objects_processed);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            file.close();
            file_size = 0;
            objects_processed = 0;
            return {};
        }
    }
    
    // NUOVO: Overload per parsing diretto da nlohmann::json con move semantics
    static std::vector<CronJob> parseJobsFromJson(json&& j, int& objects_processed) {
        std::vector<CronJob> jobs;
        objects_processed = 1; // Root object
        
        try {
            // Check if "jobs" array exists
            if (!j.contains("jobs") || !j["jobs"].is_array()) {
                throw std::runtime_error("JSON must contain 'jobs' array");
            }
            
            auto& jobs_array = j["jobs"];  // CORREZIONE: Non const per permettere move
            
            // OTTIMIZZAZIONE 1: Pre-allocazione del vector
            jobs.reserve(jobs_array.size());
            
            // Parse each job
            for (auto& job_json : jobs_array) {  // CORREZIONE: auto& invece di auto&&
                objects_processed++; // Each job object
                CronJob job;
                
                // OTTIMIZZAZIONE 3: Move semantics per stringhe
                // Required fields
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
                
                // Parse schedule with move semantics
                if (job_json.contains("schedule") && job_json["schedule"].is_object()) {
                    objects_processed++; // Schedule object
                    auto& schedule = job_json["schedule"];
                    
                    // OTTIMIZZAZIONE 4: Move per schedule fields
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
                
                // Parse conditions (optional) with move semantics
                if (job_json.contains("conditions") && job_json["conditions"].is_object()) {
                    objects_processed++; // Conditions object
                    auto& conditions = job_json["conditions"];
                    
                    // OTTIMIZZAZIONE 5: Move per conditions
                    if (conditions.contains("cpu")) {
                        job.conditions.cpu_threshold = std::move(conditions["cpu"].get_ref<std::string&>());
                    }
                    if (conditions.contains("ram")) {
                        job.conditions.ram_threshold = std::move(conditions["ram"].get_ref<std::string&>());
                    }
                    if (conditions.contains("loadavg")) {
                        job.conditions.loadavg_threshold = std::move(conditions["loadavg"].get_ref<std::string&>());
                    }
                    
                    // Parse disk conditions with move
                    if (conditions.contains("disk") && conditions["disk"].is_object()) {
                        objects_processed++; // Disk conditions object
                        for (auto& [path, threshold] : conditions["disk"].items()) {  // CORREZIONE: auto& invece di auto&&
                            job.conditions.disk_thresholds[std::move(path)] = std::move(threshold.get_ref<std::string&>());
                        }
                    }
                }
                
                // Convert to legacy format for compatibility
                parseScheduleToLegacyFormat(job);
                
                // OTTIMIZZAZIONE 6: emplace_back invece di push_back
                jobs.emplace_back(std::move(job));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            return {};
        }
        
        return jobs;
    }
    
    // Parsing da string per compatibilità (ora usa move semantics internamente)
    static std::vector<CronJob> parseJobsFromJson(const std::string& json_string, int& objects_processed) {
        try {
            // OTTIMIZZAZIONE 7: Parse diretto senza copia intermedia
            json j = json::parse(json_string);
            return parseJobsFromJson(std::move(j), objects_processed);
        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            objects_processed = 0;
            return {};
        }
    }
    
private:
    static void parseScheduleToLegacyFormat(CronJob& job) {
        // OTTIMIZZAZIONE 8: Cache delle string più comuni per evitare confronti ripetuti
        static const std::string ASTERISK = "*";
        static const std::string INTERVAL_PREFIX = "*/";
        
        // Parse minute
        if (job.schedule.minute == ASTERISK) {
            job.minute = -1; // Special value for "any minute"
        } else if (job.schedule.minute.compare(0, INTERVAL_PREFIX.length(), INTERVAL_PREFIX) == 0) {
            // Handle */X format (every X minutes)
            job.minute = -2; // Special value for interval
            // TODO: Store interval value for proper handling
        } else {
            // OTTIMIZZAZIONE 9: Fast integer parsing with error handling
            try {
                job.minute = std::stoi(job.schedule.minute);
                // Validazione range
                if (job.minute < 0 || job.minute > 59) {
                    job.minute = 0; // Safe fallback
                }
            } catch (const std::exception&) {
                job.minute = 0; // Default fallback
            }
        }
        
        // Parse hour with same optimizations
        if (job.schedule.hour == ASTERISK) {
            job.hour = -1; // Special value for "any hour"
        } else {
            try {
                job.hour = std::stoi(job.schedule.hour);
                // Validazione range
                if (job.hour < 0 || job.hour > 23) {
                    job.hour = 0; // Safe fallback
                }
            } catch (const std::exception&) {
                job.hour = 0; // Default fallback
            }
        }
        
        // OTTIMIZZAZIONE 10: Determine frequency con early returns
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
            return; // Early exit
        }
        
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
        
        if (job.schedule.month != ASTERISK) {
            job.frequency = CronFrequency::YEARLY;
            try {
                job.day_param = std::stoi(job.schedule.day_of_month);
                job.month_param = std::stoi(job.schedule.month);
                // Validate ranges
                if (job.day_param < 1 || job.day_param > 31) job.day_param = 1;
                if (job.month_param < 1 || job.month_param > 12) job.month_param = 1;
            } catch (const std::exception&) {
                job.day_param = 1;
                job.month_param = 1;
            }
            return; // Early exit
        }
        
        // Default case
        job.frequency = CronFrequency::DAILY;
        job.day_param = 0;
        job.month_param = 0;
    }
};

/**
 * Test Runner per nanoCron - VERSIONE OTTIMIZZATA con nuovo parsing
 */
int main(int argc, char* argv[]) {
    std::string jobs_file = "./test_jobs.json";
    std::string log_dir = "./test_logs";
    
    // Override del file se specificato
    if (argc > 1) {
        jobs_file = argv[1];
        std::cout << "Using jobs file from argument: " << jobs_file << std::endl;
    } else {
        std::cout << "Using default jobs file: " << jobs_file << std::endl;
    }
    
    if (argc > 2) {
        log_dir = argv[2];
        std::cout << "Using log directory from argument: " << log_dir << std::endl;
    } else {
        std::cout << "Using default log directory: " << log_dir << std::endl;
    }
    
    std::cout << "=== nanoCron Parsing Performance Test (OPTIMIZED VERSION) ===" << std::endl;
    std::cout << "Testing file: " << jobs_file << std::endl;
    std::cout << "Log directory: " << log_dir << std::endl;
    std::cout << "Optimizations: Move Semantics, Pre-allocation, Direct JSON Parsing" << std::endl;
    
    // Crea oggetto per misurare performance
    PerformanceMetrics metrics("nanoCron-Optimized", log_dir);
    
    // Variabili per validità del test
    size_t file_size = 0;
    int objects_processed = 0;
    int jobs_parsed = 0;
    bool parsing_successful = false;
    
    // Inizia misurazione
    metrics.startMeasuring();
    
    try {
        // Carica i job usando il parsing ottimizzato identico a JobConfig
        auto jobs = TestJobConfig::loadJobs(jobs_file, file_size, objects_processed);
        jobs_parsed = jobs.size();
        parsing_successful = !jobs.empty();
        
        // Simula elaborazione ottimizzata con meno overhead
        for (const auto& job : jobs) {
            // Accesso ottimizzato ai campi
            const std::string& desc = job.description;
            const std::string& cmd = job.command;
            
            // OTTIMIZZAZIONE: Evita concatenazioni di string non necessarie
            if (!desc.empty() && !cmd.empty()) {
                // Simula validazione schedule ottimizzata
                bool valid_schedule = (job.minute >= -2 && job.minute <= 59) &&
                                    (job.hour >= -1 && job.hour <= 23);
                
                if (valid_schedule) {
                    // Simula validazione condizioni ottimizzata
                    if (!job.conditions.cpu_threshold.empty()) {
                        // OTTIMIZZAZIONE: Check più veloce senza substring
                        char first_char = job.conditions.cpu_threshold[0];
                        if (first_char == '<' || first_char == '>' || first_char == '=') {
                            // Parsing condition logic ottimizzato
                        }
                    }
                }
            }
            
            // OTTIMIZZAZIONE: Delay ridotto per simulazione più realistica
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        std::cout << "Successfully loaded " << jobs.size() << " jobs (OPTIMIZED)" << std::endl;
        std::cout << "Processed " << objects_processed << " JSON objects" << std::endl;
        std::cout << "File size: " << file_size << " bytes" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error during parsing: " << e.what() << std::endl;
        parsing_successful = false;
    }
    
    // Ferma misurazione
    metrics.stopMeasuring();
    
    // Imposta metadata di validità
    metrics.setValidityMetrics(jobs_parsed, objects_processed, parsing_successful, file_size);
    
    // Mostra risultati a console
    std::cout << "\n=== OPTIMIZED RESULTS ===" << std::endl;
    std::cout << "Parse completed in " << metrics.getParseTimeMs() << " ms" << std::endl;
    std::cout << "Memory used (delta): " << metrics.getMemoryUsed() << " KB" << std::endl;
    std::cout << "Peak memory (absolute): " << metrics.getAbsolutePeakMemory() << " KB" << std::endl;
    std::cout << "Initial memory (absolute): " << metrics.getAbsoluteInitialMemory() << " KB" << std::endl;
    std::cout << "Peak CPU: " << metrics.getPeakCPU() << "%" << std::endl;
    std::cout << "Average CPU: " << metrics.getAvgCPU() << "%" << std::endl;
    
    // Log delle metriche
    metrics.logMetrics("Optimized Parse Test");
    
    if (!parsing_successful) {
        std::cout << "Warning: Parsing failed. Results may not be representative." << std::endl;
        return 1;
    }
    
    std::cout << "Test completed successfully with optimized parsing!" << std::endl;
    return 0;
}