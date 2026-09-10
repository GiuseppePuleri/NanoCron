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
    bool silent_mode;

    std::string get_timestamp();
    std::string get_level_string(LogLevel level);

    /**
     * Formats and writes a single log entry. Caller must already hold
     * log_mutex (see Logger.cpp for why this exists as a separate method).
     */
    void write_entry_locked(LogLevel level, const std::string& message, const std::string& job_name);

public:
    Logger(const std::string& filename = "logs/cron.log");
    ~Logger();

    void setSilentMode(bool silent);
    bool isSilentMode() const;

    void log(LogLevel level, const std::string& message, const std::string& job_name = "");

    void debug(const std::string& message, const std::string& job_name = "");
    void info(const std::string& message, const std::string& job_name = "");
    void warning(const std::string& message, const std::string& job_name = "");
    void error(const std::string& message, const std::string& job_name = "");
    void success(const std::string& message, const std::string& job_name = "");

    // Closes the current log file, renames it to a dated archive, and
    // opens a fresh log file in its place.
    void rotate_logs();
};

#endif // LOGGER_H