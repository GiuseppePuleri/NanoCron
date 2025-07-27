/*
 * CUSTOM CRON SYSTEM - Advanced Mini Cron
 * 
 * This file implements a daemon (background process) that simulates
 * the behavior of cron on Unix/Linux systems, allowing to schedule
 * and automatically execute commands at predetermined times.
 * 
 * USAGE INSTRUCTIONS:
 * - This is a daemon that works in the background
 * - No need to execute the script if the server goes down
 * - Compiles inside Docker without problems
 * - You MUST restart the container to save changes
 * 
 * BUILD PROCESS:
 * 1. Compilation: g++ -O2 -o mainCron mainCron.cpp
 * 2. Rebuild: docker-compose build app
 * 3. Restart: docker-compose up -d
 * 4. Check: use TOP monitor or ps aux | grep mainCron
 */

// === LIBRARY INCLUDES ===
#include <iostream>     // Standard input/output (cout, cerr)
#include <ctime>        // Date and time management
#include <chrono>       // High precision time functions
#include <thread>       // Thread management and sleep
#include <map>          // Associative container to track executions
#include <vector>       // Dynamic container for job list
#include <cstdlib>      // System functions (system())
#include <string>       // String management
#include <fstream>      // File management (read/write)
#include <sstream>      // String manipulation streams
#include <iomanip>      // Output formatting manipulators
#include <mutex>        // Thread synchronization (thread safety)
#include <filesystem>   // File system operations (C++17)

/**
 * ENUM: Supported execution frequencies
 * 
 * Defines all possible scheduling types for cron jobs.
 * Each value represents a different temporal pattern.
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
 * 
 * Defines different importance levels for log messages.
 * Allows categorizing and filtering messages based on their criticality.
 */
enum class LogLevel {
    DEBUG,      // Detailed information for debugging
    INFO,       // General information about operation
    WARNING,    // Warnings - anomalous but not critical situations
    ERROR,      // Errors - something went wrong
    SUCCESS     // Operations completed successfully
};

/**
 * STRUCT: Cron Job Definition
 * 
 * This structure contains all information needed to define
 * a scheduled job: when to execute it, with what frequency and what to execute.
 */
struct CronJob {
    int hour;               // Execution hour (0-23)
    int minute;             // Execution minute (0-59)
    CronFrequency frequency; // Frequency type (daily, weekly, etc.)
    int day_param;          // Day parameter (for weekly/monthly/yearly)
    int month_param;        // Month parameter (only for yearly)
    std::string command;    // Command to execute
    std::string description; // Job description for logs
};

/**
 * CLASS: Thread-Safe Logging System
 * 
 * This class manages all cron system logs, writing both to file
 * and console. Includes automatic log rotation functionality and
 * support for different logging levels. It's thread-safe thanks to mutex usage.
 */
class Logger {
private:
    std::string log_file;        // Main log file path
    std::ofstream log_stream;    // Stream for writing to file
    std::mutex log_mutex;        // Mutex to ensure thread safety
    
    /**
     * Generates a precise timestamp with milliseconds
     * Format: YYYY-MM-DD HH:MM:SS.mmm
     */
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        // Calculate remaining milliseconds
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    /**
     * Converts log level to readable string
     */
    std::string get_level_string(LogLevel level) {
        switch(level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR:   return "ERROR";
            case LogLevel::SUCCESS: return "SUCCESS";
            default: return "UNKNOWN";
        }
    }
    
public:
    /**
     * CONSTRUCTOR: Initializes the logging system
     * 
     * Creates the logs directory if it doesn't exist and opens the log file.
     * If it can't open the file, prints an error but continues to work
     * (will only write to console).
     */
    Logger(const std::string& filename = "logs/cron.log") : log_file(filename) {
        // Create logs directory using modern C++17 features
        try {
            std::filesystem::create_directories("logs");
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Cannot create logs directory: " << e.what() << std::endl;
        }
        
        // Open file in append mode (adds to the end)
        log_stream.open(log_file, std::ios::app);
        
        if (!log_stream.is_open()) {
            std::cerr << "FATAL: Cannot open log file: " << log_file << std::endl;
        }
    }
    
    /**
     * DESTRUCTOR: Safely closes the log file
     */
    ~Logger() {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_stream.is_open()) {
            log_stream.close();
        }
    }
    
    /**
     * MAIN METHOD: Writes a log message
     * 
     * This is the central method that formats and writes messages both to file
     * and console. It's thread-safe thanks to mutex locking.
     * 
     * @param level Message level (DEBUG, INFO, etc.)
     * @param message Message content
     * @param job_name Job name (optional, for context)
     */
    void log(LogLevel level, const std::string& message, const std::string& job_name = "") {
        std::lock_guard<std::mutex> lock(log_mutex);  // Ensures thread safety
        
        std::string timestamp = get_timestamp();
        std::string level_str = get_level_string(level);
        
        // Builds complete message in format:
        // [TIMESTAMP] [LEVEL] [JOB_NAME] MESSAGE
        std::string log_entry = "[" + timestamp + "] [" + level_str + "]";
        if (!job_name.empty()) {
            log_entry += " [" + job_name + "]";
        }
        log_entry += " " + message;
        
        // Write to file (if available)
        if (log_stream.is_open()) {
            log_stream << log_entry << std::endl;
            log_stream.flush(); // Force immediate write to disk
        }
        
        // Always write to console for real-time monitoring
        std::cout << log_entry << std::endl;
    }
    
    // === CONVENIENCE METHODS ===
    // These methods simplify logger usage for different levels
    
    void debug(const std::string& message, const std::string& job_name = "") {
        log(LogLevel::DEBUG, message, job_name);
    }
    
    void info(const std::string& message, const std::string& job_name = "") {
        log(LogLevel::INFO, message, job_name);
    }
    
    void warning(const std::string& message, const std::string& job_name = "") {
        log(LogLevel::WARNING, message, job_name);
    }
    
    void error(const std::string& message, const std::string& job_name = "") {
        log(LogLevel::ERROR, message, job_name);
    }
    
    void success(const std::string& message, const std::string& job_name = "") {
        log(LogLevel::SUCCESS, message, job_name);
    }
    
    /**
     * LOG ROTATION: Archives current log and creates a new one
     * 
     * This function is important to prevent log files from becoming
     * too large. It renames the current file with a timestamp and creates
     * a new clean file.
     */
    void rotate_logs() {
        std::lock_guard<std::mutex> lock(log_mutex);
        
        // Close current file
        if (log_stream.is_open()) {
            log_stream.close();
        }
        
        // Generate archive name based on current date
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto local = std::localtime(&time_t);
        
        std::stringstream archive_name;
        archive_name << "logs/cron_" 
                    << (local->tm_year + 1900) << "-"
                    << std::setfill('0') << std::setw(2) << (local->tm_mon + 1) << "-"
                    << std::setfill('0') << std::setw(2) << local->tm_mday << ".log";
        
        // Rename file using filesystem (safer than system())
        try {
            std::filesystem::rename(log_file, archive_name.str());
        } catch (const std::exception& e) {
            std::cerr << "Error rotating log: " << e.what() << std::endl;
        }
        
        // Reopen a new file to continue logging
        log_stream.open(log_file, std::ios::app);
        if (log_stream.is_open()) {
            info("Log rotated. Archive: " + archive_name.str());
        }
    }
};

// === GLOBAL LOGGER INSTANCE ===
// All system components will use this shared instance
Logger logger;

/**
 * FUNCTION: Gets weekday name
 * 
 * Converts day number (0=Sunday, 1=Monday, etc.) 
 * to a readable string.
 * 
 * @param wday Day number (0-6)
 * @return Day name in English
 */
std::string get_weekday_name(int wday) {
    const std::string days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", 
                               "Thursday", "Friday", "Saturday"};
    if (wday < 0 || wday > 6) return "Unknown";  // Safety check
    return days[wday];
}

/**
 * CORE FUNCTION: Determines if a job should be executed
 * 
 * This is the central cron logic: analyzes current time,
 * job frequency and last execution to decide if
 * the job should start now.
 * 
 * @param job The job to check
 * @param local Current local time
 * @param last_exec Map of last executions to avoid duplicates
 * @return true if job should be executed
 */
bool should_run_job(const CronJob& job, const std::tm& local, 
                    const std::map<std::string, std::pair<int, int>>& last_exec) {
    
    // CHECK 1: Verify hour and minute
    // If it's not the right time, don't execute
    if (job.hour != local.tm_hour || job.minute != local.tm_min) {
        return false;
    }
    
    // CHECK 2: Avoid duplicate executions in the same minute
    // Search if this job was already executed in this minute
    auto it = last_exec.find(job.command);
    if (it != last_exec.end() && 
        it->second.first == local.tm_hour && 
        it->second.second == local.tm_min) {
        logger.debug("Job already executed this minute", job.description);
        return false;
    }
    
    // CHECK 3: Verify specific frequency
    // Each frequency type has its own rules
    switch (job.frequency) {
        case CronFrequency::DAILY:
            // Every day: if it's the right time, execute
            return true;
            
        case CronFrequency::WEEKLY:
            // Weekly: check that it's the right day of the week
            return local.tm_wday == job.day_param;
            
        case CronFrequency::MONTHLY:
            // Monthly: check that it's the right day of the month
            return local.tm_mday == job.day_param;
            
        case CronFrequency::YEARLY:
            // Yearly: check both day AND month
            return local.tm_mday == job.day_param && 
                   (local.tm_mon + 1) == job.month_param;
            
        case CronFrequency::WEEKDAY:
            // Only weekdays: Monday(1) - Friday(5)
            return local.tm_wday >= 1 && local.tm_wday <= 5;
            
        case CronFrequency::WEEKEND:
            // Only weekends: Sunday(0) and Saturday(6)
            return local.tm_wday == 0 || local.tm_wday == 6;
            
        default:
            // Unknown frequency: don't execute for safety
            return false;
    }
}

/**
 * FUNCTION: Prints job configuration
 * 
 * Shows in readable format when and how a job will be executed.
 * Useful for verifying configuration at system startup.
 * 
 * @param job The job whose information to print
 */
void print_job_schedule(const CronJob& job) {
    std::stringstream ss;
    ss << "Job: " << job.command << " (" << job.description << ")\n";
    
    // Format time with zero padding for minutes < 10
    ss << "  Time: " << job.hour << ":" << (job.minute < 10 ? "0" : "") << job.minute << "\n";
    
    // Explain frequency in human-readable way
    switch (job.frequency) {
        case CronFrequency::DAILY:
            ss << "  Frequency: Every day";
            break;
        case CronFrequency::WEEKLY:
            ss << "  Frequency: Every " << get_weekday_name(job.day_param);
            break;
        case CronFrequency::MONTHLY:
            ss << "  Frequency: Day " << job.day_param << " of every month";
            break;
        case CronFrequency::YEARLY:
            ss << "  Frequency: " << job.day_param << "/" << job.month_param << " every year";
            break;
        case CronFrequency::WEEKDAY:
            ss << "  Frequency: Weekdays only (Mon-Fri)";
            break;
        case CronFrequency::WEEKEND:
            ss << "  Frequency: Weekends only (Sat-Sun)";
            break;
    }
    
    logger.info(ss.str());
}

/**
 * ADVANCED FUNCTION: Executes job with timeout and error handling
 * 
 * This function improves command execution by adding:
 * - Timeout to avoid jobs that get stuck
 * - Relative path resolution
 * - Execution time measurement
 * - Detailed error handling
 * 
 * @param command The command to execute
 * @param timeout_seconds Maximum timeout (default: 5 minutes)
 * @return Command exit code
 */
int execute_job_with_timeout(const std::string& command, int timeout_seconds = 300) {
    auto start_time = std::chrono::steady_clock::now();
    
    // IMPROVEMENT 1: Resolve relative paths to absolute
    // If command starts with "./" convert it to absolute path
    std::string full_command = command;
    if (command.find("./") == 0) {
        try {
            auto current_path = std::filesystem::current_path();
            full_command = current_path.string() + "/" + command.substr(2);
        } catch (const std::exception& e) {
            logger.warning("Could not resolve absolute path for: " + command);
        }
    }
    
    // IMPROVEMENT 2: Try to use system timeout
    // The 'timeout' command limits execution time
    std::string timed_command = "timeout " + std::to_string(timeout_seconds) + " " + full_command;
    int result = std::system(timed_command.c_str());
    
    // FALLBACK: If timeout is not available, execute without limits
    // On some systems the timeout command might not exist
    if (result == 127 * 256) {  // system() returns exit_code * 256
        logger.warning("timeout command not available, executing without timeout");
        result = std::system(full_command.c_str());
    }
    
    // MEASUREMENT: Calculate how long it took
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // ERROR HANDLING: Interpret different exit codes
    if (result == 124 * 256) { // 124 is timeout exit code
        logger.error("Job timed out after " + std::to_string(timeout_seconds) + " seconds", full_command);
    } else if (result == -1) {
        logger.error("Failed to execute command", full_command);
    } else {
        logger.debug("Job execution time: " + std::to_string(duration.count()) + " seconds", full_command);
    }
    
    return result;
}

/**
 * MAIN FUNCTION: Heart of the cron system
 * 
 * This is the main function that:
 * 1. Configures all jobs
 * 2. Enters an infinite loop
 * 3. Checks every 20 seconds if there are jobs to execute
 * 4. Handles maintenance operations (log rotation, debug)
 */
int main() {
    logger.info("=== ADVANCED MINI CRON STARTED ===");
    
    // === JOB CONFIGURATION ===
    // This is where you define all jobs you want to schedule
    std::vector<CronJob> jobs = {
        // FORMAT: {HOUR, MINUTE, FREQUENCY, DAY/MONTH, MONTH, COMMAND, DESCRIPTION}
        
        // Daily job: session cleanup at 23:00 every day
        {23, 0, CronFrequency::DAILY, 0, 0, "./Jobs/closeSessionJob", "Daily session cleanup"},
        
        // Monthly job: XML generation on 1st of every month at 05:00
        {5, 0, CronFrequency::MONTHLY, 1, 0, "./Jobs/makeAttendanceJob", "Monthly xml generation"},
        
        // Monthly job: PDF generation on 1st of every month at 01:00
        {1, 0, CronFrequency::MONTHLY, 1, 0, "./Jobs/makeReportJob", "Monthly pdf generation"},
        
        // ADD YOUR NEW JOBS HERE!
        // Examples:
        // {9, 30, CronFrequency::WEEKDAY, 0, 0, "./Jobs/backupJob", "Weekday backup"},
        // {12, 0, CronFrequency::WEEKLY, 1, 0, "./Jobs/reportJob", "Monday weekly report"},
    };
    
    // Map to track last executions (avoids duplicates)
    std::map<std::string, std::pair<int, int>> last_execution;
    
    // === CONFIGURATION PRINTOUT ===
    // At startup, show all configured jobs for verification
    logger.info("Configured jobs:");
    for (const auto& job : jobs) {
        print_job_schedule(job);
    }
    logger.info("===================================");
    
    // === VARIABLES FOR PERIODIC CHECKS ===
    // Avoid doing certain operations too many times
    int last_rotation_day = -1;    // For daily log rotation
    int last_debug_hour = -1;      // For periodic debug messages
    
    // === MAIN LOOP ===
    // This loop never ends: it's the daemon's heart
    while (true) {
        // Get current system time
        std::time_t now = std::time(nullptr);
        
        // Convert to local time in thread-safe way
        std::tm local_time;
        #ifdef _WIN32
            localtime_s(&local_time, &now);  // Windows version
        #else
            localtime_r(&now, &local_time);  // Unix/Linux version
        #endif
        
        // === MAINTENANCE 1: DAILY LOG ROTATION ===
        // At midnight (00:00) of each new day, archive logs
        if (local_time.tm_mday != last_rotation_day && 
            local_time.tm_hour == 0 && local_time.tm_min == 0) {
            logger.rotate_logs();
            last_rotation_day = local_time.tm_mday;
        }
        
        // === MAINTENANCE 2: PERIODIC DEBUG INFO ===
        // Every 4 hours print a message to show the system is working
        if (local_time.tm_hour != last_debug_hour && local_time.tm_hour % 4 == 0) {
            std::stringstream ss;
            ss << "Current time: " << local_time.tm_hour << ":" 
               << (local_time.tm_min < 10 ? "0" : "") << local_time.tm_min
               << " - " << get_weekday_name(local_time.tm_wday) 
               << " " << local_time.tm_mday << "/" << (local_time.tm_mon + 1) 
               << "/" << (local_time.tm_year + 1900) << " - System running normally";
            logger.debug(ss.str());
            last_debug_hour = local_time.tm_hour;
        }
        
        // === JOB CHECK AND EXECUTION ===
        // This is the highlight: check each job to see if it should start
        for (const auto& job : jobs) {
            if (should_run_job(job, local_time, last_execution)) {
                logger.info("Starting job: " + job.command, job.description);
                
                // Measure execution time
                auto start_time = std::chrono::steady_clock::now();
                int result = execute_job_with_timeout(job.command, 300); // 5 minutes timeout
                auto end_time = std::chrono::steady_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
                
                // Analyze result and log appropriately
                if (result == 0) {
                    // Success: job finished without errors
                    logger.success("Job completed successfully in " + std::to_string(duration.count()) + " seconds", job.description);
                } else if (result == 124 * 256) {
                    // Timeout: job took too long
                    logger.error("Job timed out after 300 seconds", job.description);
                } else {
                    // Other errors: job failed for some reason
                    logger.error("Job failed with exit code " + std::to_string(result / 256) + 
                               " after " + std::to_string(duration.count()) + " seconds", job.description);
                }
                
                // Mark this job as executed to avoid duplicates
                last_execution[job.command] = {local_time.tm_hour, local_time.tm_min};
            }
        }
        
        // === PAUSE ===
        // Wait 20 seconds before next check
        // This compromise offers good responsiveness without wasting CPU resources
        std::this_thread::sleep_for(std::chrono::seconds(20));
    }
    
    // This line is never reached because the loop is infinite
    return 0;
}
