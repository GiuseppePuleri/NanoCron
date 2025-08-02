#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <atomic>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <cstring>
#include <regex>

/**
 * Struttura per rappresentare un job crontab (equivalente al JSON)
 */
struct CrontabJob {
    std::string minute;
    std::string hour;
    std::string day_of_month;
    std::string month;
    std::string day_of_week;
    std::string command;
    std::string description;
    
    // Campi derivati per compatibilità
    int minute_int;
    int hour_int;
    int day_param;
    int month_param;
    
    CrontabJob() : minute_int(-1), hour_int(-1), day_param(0), month_param(0) {}
};

/**
 * Classe per misurare le performance del parsing crontab (SOLO PARSING)
 */
class CrontabParsingMetrics {
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
    
    // Metadata di validità del test (equivalente al nanoCron)
    struct TestValidityMetrics {
        int crontab_jobs_parsed;
        int crontab_lines_processed;
        bool successful_parsing;
        double test_duration_ms;
        std::string parsing_method;
        size_t crontab_file_size_bytes;
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
    
    // UNIFICATA: Usa /proc/self/status come nanoCron
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
    
    // UNIFICATA: CPU usage del processo corrente (identico a nanoCron)
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
    CrontabParsingMetrics(const std::string& name, const std::string& log_dir = "./test_logs") : 
        test_name(name), peak_memory(0), monitoring(false), peak_cpu_usage(0.0), 
        avg_cpu_usage(0.0), log_directory(log_dir) {
        
        createLogDirectory(log_directory);
        
        // Inizializza metrics di validità
        validity_metrics = {0, 0, false, 0.0, "Crontab Parse", 0};
        
        std::cout << "CrontabParsingMetrics (PARSING ONLY) initialized for " << name << " with log directory: " << log_directory << std::endl;
    }
    
    ~CrontabParsingMetrics() {
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
        cpu_monitor_thread = std::thread(&CrontabParsingMetrics::monitorProcess, this);
        
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
    
    // Imposta metadata di validità
    void setValidityMetrics(int jobs_parsed, int lines_processed, bool success, size_t file_size) {
        validity_metrics.crontab_jobs_parsed = jobs_parsed;
        validity_metrics.crontab_lines_processed = lines_processed;
        validity_metrics.successful_parsing = success;
        validity_metrics.crontab_file_size_bytes = file_size;
    }
    
    void logMetrics(const std::string& operation = "Parse") {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::string final_log_path = normalizePath(log_directory, "performance.log");
        
        std::cout << "Crontab parsing metrics logged to " << final_log_path << std::endl;
        
        std::ofstream log(final_log_path, std::ios::app);
        if (log.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            log << "=== " << test_name << " " << operation << " Metrics (PARSING ONLY) ===" << std::endl;
            log << "Timestamp: " << std::ctime(&time_t);
            log << "Parse Time: " << duration.count() << " microseconds" << std::endl;
            log << "Parse Time (ms): " << duration.count() / 1000.0 << " ms" << std::endl;
            log << "Initial Memory: " << initial_memory << " KB" << std::endl;
            log << "Peak Memory: " << peak_memory << " KB" << std::endl;
            log << "Memory Used: " << getMemoryUsed() << " KB" << std::endl;
            log << "Peak CPU Usage: " << peak_cpu_usage << "%" << std::endl;
            log << "Average CPU Usage: " << avg_cpu_usage << "%" << std::endl;
            log << "CPU Samples: " << cpu_samples.size() << std::endl;
            
            // Metadata di validità (equivalente al nanoCron)
            log << "--- Test Validity Metrics ---" << std::endl;
            log << "Parsing Method: " << validity_metrics.parsing_method << std::endl;
            log << "Crontab Jobs Parsed: " << validity_metrics.crontab_jobs_parsed << std::endl;
            log << "Crontab Lines Processed: " << validity_metrics.crontab_lines_processed << std::endl;
            log << "Successful Parsing: " << (validity_metrics.successful_parsing ? "YES" : "NO") << std::endl;
            log << "Crontab File Size: " << validity_metrics.crontab_file_size_bytes << " bytes" << std::endl;
            log << "Test Duration: " << validity_metrics.test_duration_ms << " ms" << std::endl;
            log << "Memory Measurement: /proc/self/status (UNIFIED)" << std::endl;
            log << "CPU Measurement: clock() current process (UNIFIED)" << std::endl;
            log << "----------------------------------------" << std::endl;
            log.close();
        } else {
            std::cerr << "Error: Could not open log file: " << final_log_path << std::endl;
        }
    }
};

/**
 * Parser crontab che replica la logica del parsing nanoCron (ma per formato crontab)
 */
class CrontabParser {
public:
    static std::vector<CrontabJob> loadJobsFromCrontab(const std::string& filename, size_t& file_size, int& lines_processed) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open " << filename << std::endl;
            file_size = 0;
            lines_processed = 0;
            return std::vector<CrontabJob>();
        }
        
        // Calcola dimensione file
        file.seekg(0, std::ios::end);
        file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<CrontabJob> jobs;
        std::string line;
        lines_processed = 0;
        
        try {
            while (std::getline(file, line)) {
                lines_processed++;
                
                // Ignora commenti e righe vuote
                if (line.empty() || line[0] == '#' || line.find('=') != std::string::npos) {
                    continue;
                }
                
                // Parsing della riga crontab
                CrontabJob job = parseCrontabLine(line);
                if (!job.command.empty()) {
                    jobs.push_back(job);
                }
            }
            
            file.close();
        } catch (const std::exception& e) {
            std::cerr << "Error parsing crontab: " << e.what() << std::endl;
            file.close();
            file_size = 0;
            lines_processed = 0;
            return std::vector<CrontabJob>();
        }
        
        return jobs;
    }
    
    static std::vector<CrontabJob> parseFromString(const std::string& crontab_content, int& lines_processed) {
        std::vector<CrontabJob> jobs;
        std::istringstream stream(crontab_content);
        std::string line;
        lines_processed = 0;
        
        try {
            while (std::getline(stream, line)) {
                lines_processed++;
                
                // Ignora commenti e righe vuote
                if (line.empty() || line[0] == '#' || line.find('=') != std::string::npos) {
                    continue;
                }
                
                // Parsing della riga crontab
                CrontabJob job = parseCrontabLine(line);
                if (!job.command.empty()) {
                    jobs.push_back(job);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing crontab string: " << e.what() << std::endl;
            return std::vector<CrontabJob>();
        }
        
        return jobs;
    }
    
private:
    static CrontabJob parseCrontabLine(const std::string& line) {
        CrontabJob job;
        
        // Trim whitespace
        std::string trimmed_line = line;
        trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
        trimmed_line.erase(trimmed_line.find_last_not_of(" \t") + 1);
        
        if (trimmed_line.empty()) {
            return job; // Empty job
        }
        
        // Split della riga in campi
        std::vector<std::string> fields;
        std::istringstream iss(trimmed_line);
        std::string field;
        
        while (iss >> field) {
            fields.push_back(field);
        }
        
        // Una riga crontab valida deve avere almeno 6 campi (5 time + comando)
        if (fields.size() < 6) {
            return job; // Invalid line
        }
        
        // Parsing dei campi temporali
        job.minute = fields[0];
        job.hour = fields[1];
        job.day_of_month = fields[2];
        job.month = fields[3];
        job.day_of_week = fields[4];
        
        // Il comando è tutto il resto unito
        std::ostringstream cmd_stream;
        for (size_t i = 5; i < fields.size(); i++) {
            if (i > 5) cmd_stream << " ";
            cmd_stream << fields[i];
        }
        job.command = cmd_stream.str();
        
        // Genera una descrizione basata sul comando
        job.description = generateDescription(job.command);
        
        // Converte i campi temporali a formato legacy (simile al nanoCron)
        convertToLegacyFormat(job);
        
        return job;
    }
    
    static std::string generateDescription(const std::string& command) {
        // Estrai il nome del file dal path
        size_t last_slash = command.find_last_of('/');
        if (last_slash != std::string::npos && last_slash + 1 < command.length()) {
            std::string filename = command.substr(last_slash + 1);
            // Trova il primo spazio per separare il comando dagli argomenti
            size_t space_pos = filename.find(' ');
            if (space_pos != std::string::npos) {
                filename = filename.substr(0, space_pos);
            }
            return "Crontab job: " + filename;
        }
        return "Crontab job";
    }
    
    static void convertToLegacyFormat(CrontabJob& job) {
        // Converte il formato crontab a formato legacy (simile al nanoCron)
        
        // Parse minute
        if (job.minute == "*") {
            job.minute_int = -1; // Special value for "any minute"
        } else if (job.minute.find("*/") == 0) {
            job.minute_int = -2; // Special value for interval
        } else {
            try {
                job.minute_int = std::stoi(job.minute);
            } catch (const std::exception&) {
                job.minute_int = 0; // Default fallback
            }
        }
        
        // Parse hour
        if (job.hour == "*") {
            job.hour_int = -1; // Special value for "any hour"
        } else {
            try {
                job.hour_int = std::stoi(job.hour);
            } catch (const std::exception&) {
                job.hour_int = 0; // Default fallback
            }
        }
        
        // Determine frequency based on schedule (simile al nanoCron)
        if (job.day_of_week != "*") {
            try {
                job.day_param = std::stoi(job.day_of_week);
            } catch (const std::exception&) {
                job.day_param = 0;
            }
        } else if (job.day_of_month != "*") {
            try {
                job.day_param = std::stoi(job.day_of_month);
            } catch (const std::exception&) {
                job.day_param = 1;
            }
        } else if (job.month != "*") {
            try {
                job.day_param = std::stoi(job.day_of_month);
                job.month_param = std::stoi(job.month);
            } catch (const std::exception&) {
                job.day_param = 1;
                job.month_param = 1;
            }
        } else {
            job.day_param = 0;
            job.month_param = 0;
        }
    }
};

/**
 * Genera un file crontab equivalente al test_jobs.json
 */
bool generateEquivalentCrontab(const std::string& filename) {
    std::ofstream crontab_file(filename);
    if (!crontab_file.is_open()) {
        std::cerr << "Failed to create equivalent crontab file: " << filename << std::endl;
        return false;
    }
    
    // Header del crontab
    crontab_file << "# Equivalent crontab to test_jobs.json for fair performance comparison\n";
    crontab_file << "# Contains same 10 jobs as JSON version\n";
    crontab_file << "SHELL=/bin/bash\n";
    crontab_file << "PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin\n";
    crontab_file << "\n";
    
    // 10 jobs equivalenti al JSON (ogni minuto)
    for (int i = 1; i <= 10; i++) {
        crontab_file << "* * * * * /home/giuseppe/code/NanoCron-v3/init/jobs/makeD" << i << "\n";
    }
    
    crontab_file.close();
    std::cout << "Generated equivalent crontab with 10 jobs: " << filename << std::endl;
    return true;
}

/**
 * Test Runner per System Cron - misura SOLO il parsing (equivalente al nanoCron)
 */
int main(int argc, char* argv[]) {
    std::string crontab_file = "./test_jobs.crontab";
    std::string log_dir = "./test_logs";
    
    // Override dei parametri se specificati
    if (argc > 1) {
        log_dir = argv[1];
        std::cout << "Using log directory from argument: " << log_dir << std::endl;
    } else {
        std::cout << "Using default log directory: " << log_dir << std::endl;
    }
    
    std::cout << "=== System Cron Parsing Performance Test (PARSING ONLY - FAIR COMPARISON) ===" << std::endl;
    std::cout << "Testing crontab file: " << crontab_file << std::endl;
    std::cout << "Log directory: " << log_dir << std::endl;
    
    // Genera il file crontab equivalente
    if (!generateEquivalentCrontab(crontab_file)) {
        std::cerr << "Failed to generate equivalent crontab file" << std::endl;
        return 1;
    }
    
    // Crea oggetto per misurare performance
    CrontabParsingMetrics metrics("System Cron", log_dir);
    
    // Variabili per validità del test
    size_t file_size = 0;
    int lines_processed = 0;
    int jobs_parsed = 0;
    bool parsing_successful = false;
    
    // Inizia misurazione
    metrics.startMeasuring();
    
    try {
        // Carica i job usando il parsing del crontab (equivalente al JSON parsing)
        auto jobs = CrontabParser::loadJobsFromCrontab(crontab_file, file_size, lines_processed);
        jobs_parsed = jobs.size();
        parsing_successful = !jobs.empty();
        
        // Simula un po' di elaborazione aggiuntiva per rendere il test equivalente al nanoCron
        for (const auto& job : jobs) {
            // Accesso a tutti i campi per simulare uso reale (come nanoCron)
            std::string info = job.description + " | " + job.command;
            info += " | " + job.minute + ":" + job.hour;
            
            // Simula validazione (equivalente al nanoCron)
            if (job.minute_int >= 0 && job.minute_int <= 59) {
                // Simula parsing validation logic
            }
            if (job.hour_int >= 0 && job.hour_int <= 23) {
                // Simula parsing validation logic
            }
            
            // Piccolo delay per simulare elaborazione (equivalente al nanoCron)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        std::cout << "Successfully loaded " << jobs.size() << " crontab jobs" << std::endl;
        std::cout << "Processed " << lines_processed << " crontab lines" << std::endl;
        std::cout << "File size: " << file_size << " bytes" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error during crontab parsing: " << e.what() << std::endl;
        parsing_successful = false;
    }
    
    // Ferma misurazione
    metrics.stopMeasuring();
    
    // Imposta metadata di validità
    metrics.setValidityMetrics(jobs_parsed, lines_processed, parsing_successful, file_size);
    
    // Mostra risultati a console
    std::cout << "\n=== RESULTS ===" << std::endl;
    std::cout << "Crontab parsing completed in " << metrics.getParseTimeMs() << " ms" << std::endl;
    std::cout << "Memory used (delta): " << metrics.getMemoryUsed() << " KB" << std::endl;
    std::cout << "Peak memory (absolute): " << metrics.getAbsolutePeakMemory() << " KB" << std::endl;
    std::cout << "Initial memory (absolute): " << metrics.getAbsoluteInitialMemory() << " KB" << std::endl;
    std::cout << "Peak CPU: " << metrics.getPeakCPU() << "%" << std::endl;
    std::cout << "Average CPU: " << metrics.getAvgCPU() << "%" << std::endl;
    
    // Log delle metriche
    metrics.logMetrics("Crontab Parse Test");
    
    // Cleanup del file temporaneo
    unlink(crontab_file.c_str());
    
    if (!parsing_successful) {
        std::cout << "Warning: Crontab parsing failed. Results may not be representative." << std::endl;
        return 1;
    }
    
    std::cout << "Crontab parsing test completed successfully!" << std::endl;
    return 0;
}