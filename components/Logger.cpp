#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

Logger::Logger(const std::string& filename) : log_file(filename), silent_mode(false) {
    // Create parent directory of the log file
    try {
        std::filesystem::path log_path(filename);
        std::filesystem::create_directories(log_path.parent_path());
    } catch (const std::exception& e) {
        std::cerr << "WARNING: Cannot create log directory: " << e.what() << std::endl;
    }
    
    // Open log file in append mode
    log_stream.open(log_file, std::ios::app);
    
    if (!log_stream.is_open()) {
        std::cerr << "FATAL: Cannot open log file: " << log_file << std::endl;
    }
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_stream.is_open()) {
        log_stream.close();
    }
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::get_level_string(LogLevel level) {
    switch(level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::SUCCESS: return "SUCCESS";
        default: return "UNKNOWN";
    }
}

void Logger::setSilentMode(bool silent) {
    std::lock_guard<std::mutex> lock(log_mutex);
    silent_mode = silent;
}

bool Logger::isSilentMode() const {
    return silent_mode;
}

void Logger::log(LogLevel level, const std::string& message, const std::string& job_name) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    std::string timestamp = get_timestamp();
    std::string level_str = get_level_string(level);
    
    std::string log_entry = "[" + timestamp + "] [" + level_str + "]";
    if (!job_name.empty()) {
        log_entry += " [" + job_name + "]";
    }
    log_entry += " " + message;
    
    // Always write to file
    if (log_stream.is_open()) {
        log_stream << log_entry << std::endl;
        log_stream.flush();
    }
    
    // Write to console only if not in silent mode
    if (!silent_mode) {
        std::cout << log_entry << std::endl;
    }
}

void Logger::debug(const std::string& message, const std::string& job_name) {
    log(LogLevel::DEBUG, message, job_name);
}

void Logger::info(const std::string& message, const std::string& job_name) {
    log(LogLevel::INFO, message, job_name);
}

void Logger::warning(const std::string& message, const std::string& job_name) {
    log(LogLevel::WARNING, message, job_name);
}

void Logger::error(const std::string& message, const std::string& job_name) {
    log(LogLevel::ERROR, message, job_name);
}

void Logger::success(const std::string& message, const std::string& job_name) {
    log(LogLevel::SUCCESS, message, job_name);
}

void Logger::rotate_logs() {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    if (log_stream.is_open()) {
        log_stream.close();
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local = std::localtime(&time_t);
    
    std::stringstream archive_name;
    archive_name << "logs/cron_" 
                << (local->tm_year + 1900) << "-"
                << std::setfill('0') << std::setw(2) << (local->tm_mon + 1) << "-"
                << std::setfill('0') << std::setw(2) << local->tm_mday << ".log";
    
    try {
        std::filesystem::rename(log_file, archive_name.str());
    } catch (const std::exception& e) {
        std::cerr << "Error rotating log: " << e.what() << std::endl;
    }
    
    log_stream.open(log_file, std::ios::app);
    if (log_stream.is_open()) {
        info("Log rotated. Archive: " + archive_name.str());
    }
}