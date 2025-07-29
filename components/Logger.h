#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include "CronTypes.h"

/**
 * Logger Class - Thread-Safe Logging System
 * 
 * Manages all system logging with file and console output,
 * automatic log rotation, and multiple logging levels.
 */
class Logger {
private:
    std::string log_file;
    std::ofstream log_stream;
    std::mutex log_mutex;
    bool silent_mode;  // New: controls console output
    
    std::string get_timestamp();
    std::string get_level_string(LogLevel level);
    
public:
    Logger(const std::string& filename = "logs/cron.log");
    ~Logger();
    
    // New: control console output
    void setSilentMode(bool silent);
    bool isSilentMode() const;
    
    // Main logging method
    void log(LogLevel level, const std::string& message, const std::string& job_name = "");
    
    // Convenience methods for different log levels
    void debug(const std::string& message, const std::string& job_name = "");
    void info(const std::string& message, const std::string& job_name = "");
    void warning(const std::string& message, const std::string& job_name = "");
    void error(const std::string& message, const std::string& job_name = "");
    void success(const std::string& message, const std::string& job_name = "");
    
    // Log rotation functionality
    void rotate_logs();
};

#endif // LOGGER_H