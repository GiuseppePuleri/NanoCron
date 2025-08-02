/**
 * @file nanoCronCLI.cpp
 * @brief Interactive command-line interface for nanoCron daemon management
 * 
 * Provides user-friendly commands for daemon control, job management,
 * and system monitoring with colored terminal output and enhanced UX.
 */

#include <iostream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h> 

/**
 * @brief ANSI color codes for enhanced terminal output
 * 
 * These constants enable colored console output to improve readability
 * and user experience by providing visual distinction between different
 * message types (success, error, warning, info).
 */
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string CYAN = "\033[36m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";

/**
 * @brief Utility functions for colored terminal output
 * @param msg Message to display with color formatting
 * 
 * These functions wrap console output with ANSI color codes
 * to provide consistent visual feedback throughout the CLI.
 */
void printSuccess(const std::string& msg) {
    std::cout << GREEN << msg << RESET << "\n";
}

void printError(const std::string& msg) {
    std::cerr << RED << msg << RESET << "\n";
}

void printWarning(const std::string& msg) {
    std::cout << YELLOW << msg << RESET << "\n";
}

void printInfo(const std::string& msg) {
    std::cout << CYAN << msg << RESET << "\n";
}

/**
 * @brief Animated ASCII art display function
 * @param art ASCII art string to display
 * @param delayMs Delay in milliseconds between each character
 * 
 * Creates a typewriter effect for displaying ASCII art banners,
 * enhancing the visual appeal of the CLI startup sequence.
 */
void printAsciiArtGradually(const std::string& art, int delayMs = 50) {
    for (char c : art) {
        std::cout << c << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    std::cout << std::endl;
}

/**
 * @brief Reads jobs.json path from configuration environment file
 * @return String containing the path to jobs.json file
 * 
 * Implements dynamic path resolution by parsing the config.env file.
 * This enables flexible deployment scenarios where job configuration
 * files may be located in different directories. Falls back to
 * "./jobs.json" if configuration is unavailable.
 */
std::string getJobsJsonPath() {
    const std::string CONFIG_FILE = "/opt/nanoCron/init/config.env";
    std::ifstream configFile(CONFIG_FILE);
    
    if (!configFile.is_open()) {
        printWarning("Cannot open config file: " + CONFIG_FILE);
        printInfo("Falling back to default: ./jobs.json");
        return "./jobs.json";
    }
    
    std::string line;
    // Parse configuration file line by line
    while (std::getline(configFile, line)) {
        // Search for the jobs path configuration directive
        if (line.find("ORIGINAL_JOBS_JSON_PATH=") == 0) {
            std::string path = line.substr(24); // Remove "ORIGINAL_JOBS_JSON_PATH=" prefix
            configFile.close();
            return path;
        }
    }
    
    configFile.close();
    printWarning("ORIGINAL_JOBS_JSON_PATH not found in config file");
    printInfo("Falling back to default: ./jobs.json");
    return "./jobs.json";
}

/**
 * @brief Reads log file path from configuration environment file
 * @return String containing the path to cron.log file
 * 
 * Similar to getJobsJsonPath() but for log file location resolution.
 * Enables flexible log file placement for different deployment contexts.
 */
std::string getCronLogPath() {
    const std::string CONFIG_FILE = "/opt/nanoCron/init/config.env";
    std::ifstream configFile(CONFIG_FILE);
    
    if (!configFile.is_open()) {
        printWarning("Cannot open config file: " + CONFIG_FILE);
        printInfo("Falling back to default: ./logs/cron.log");
        return "./logs/cron.log";
    }
    
    std::string line;
    while (std::getline(configFile, line)) {
        // Note: Correct prefix length is 23 characters for "ORIGINAL_CRON_LOG_PATH="
        if (line.find("ORIGINAL_CRON_LOG_PATH=") == 0) {
            std::string path = line.substr(23);
            configFile.close();
            return path;
        }
    }
    
    configFile.close();
    printWarning("ORIGINAL_CRON_LOG_PATH not found in config file");
    printInfo("Falling back to default: ./logs/cron.log");
    return "./logs/cron.log";
}

/**
 * @brief Enhanced daemon status detection with PID resolution
 * @return Pair<bool, int> where first element indicates if daemon is running,
 *         second element contains the daemon PID (or -1 if not running)
 * 
 * Implements robust process detection using pgrep with filtering to distinguish
 * between the actual daemon process and the CLI tool. Uses process name
 * verification to ensure accuracy and prevent false positives.
 */
std::pair<bool, int> getDaemonStatus() {
    // Method 1: Search for nanoCron process excluding current CLI instance
    std::string cmd = "pgrep -f '^/usr/local/bin/nanoCron$' 2>/dev/null || pgrep -f 'nanoCron$' 2>/dev/null | grep -v " + std::to_string(getpid());
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {false, -1};
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    if (result.empty()) {
        return {false, -1};
    }
    
    try {
        // Extract first PID from potentially multiple results
        std::istringstream iss(result);
        std::string firstPid;
        iss >> firstPid;
        
        int pid = std::stoi(firstPid);
        
        /**
         * Verification step: Confirm the process is actually nanoCron daemon
         * and not nanoCronCLI or another similarly named process.
         * This prevents false positives in process detection.
         */
        std::string checkCmd = "ps -p " + std::to_string(pid) + " -o comm= 2>/dev/null";
        FILE* checkPipe = popen(checkCmd.c_str(), "r");
        if (checkPipe) {
            char procName[128];
            if (fgets(procName, sizeof(procName), checkPipe)) {
                std::string name(procName);
                name.erase(name.find_last_not_of(" \n\r\t") + 1); // trim whitespace
                pclose(checkPipe);
                
                if (name == "nanoCron") {
                    return {true, pid};
                }
            } else {
                pclose(checkPipe);
            }
        }
        
        return {false, -1};
    } catch (...) {
        return {false, -1};
    }
}

/**
 * @brief Simplified daemon running check
 * @return Boolean indicating if nanoCron daemon is currently running
 * 
 * Convenience wrapper around getDaemonStatus() for cases where
 * only the running state is needed, not the specific PID.
 */
bool isDaemonRunning() {
    auto [running, pid] = getDaemonStatus();
    return running;
}

/**
 * @brief Converts cron schedule notation to human-readable text
 * @param minute Minute field from cron schedule
 * @param hour Hour field from cron schedule  
 * @param day_of_month Day of month field from cron schedule
 * @param month Month field from cron schedule
 * @param day_of_week Day of week field from cron schedule
 * @return Human-readable schedule description
 * 
 * Translates cron time specifications into natural language descriptions
 * to improve user comprehension of job scheduling. Handles special values
 * like "*" (any) and provides contextual formatting.
 */
std::string scheduleToText(const std::string& minute, const std::string& hour, 
                          const std::string& day_of_month, const std::string& month, 
                          const std::string& day_of_week) {
    std::string result = "Runs ";
    
    // Process minute specification
    if (minute == "*") {
        result += "every minute";
    } else if (minute == "0") {
        result += "at minute 0";
    } else {
        result += "at minute " + minute;
    }
    
    // Process hour specification with context-aware formatting
    if (hour == "*") {
        if (minute != "*") result += " of every hour";
    } else {
        result += " at " + hour + ":";
        result += (minute == "*") ? "XX" : (minute.length() == 1 ? "0" + minute : minute);
    }
    
    // Process day of month specification
    if (day_of_month != "*") {
        result += " on day " + day_of_month;
    }
    
    // Process month specification with name lookup
    if (month != "*") {
        std::vector<std::string> months = {"", "January", "February", "March", "April", "May", "June",
                                          "July", "August", "September", "October", "November", "December"};
        try {
            int monthNum = std::stoi(month);
            if (monthNum >= 1 && monthNum <= 12) {
                result += " in " + months[monthNum];
            }
        } catch (...) {
            result += " in month " + month;
        }
    }
    
    // Process day of week specification with name lookup
    if (day_of_week != "*") {
        std::vector<std::string> days = {"Sunday", "Monday", "Tuesday", "Wednesday", 
                                        "Thursday", "Friday", "Saturday"};
        try {
            int dayNum = std::stoi(day_of_week);
            if (dayNum >= 0 && dayNum <= 6) {
                result += " on " + days[dayNum];
            }
        } catch (...) {
            result += " on day " + day_of_week;
        }
    }
    
    return result;
}

/**
 * @brief Parses and displays job configuration in user-friendly format
 * 
 * Implements a lightweight JSON parser specifically for job configuration display.
 * Extracts job details including schedule, conditions, and descriptions,
 * then formats them for easy reading. Uses basic string parsing instead of
 * a full JSON library to minimize dependencies for the CLI tool.
 */
void seeJobs() {
    printInfo("[seejobs] Current job configuration:");
    
    std::string jobsPath = getJobsJsonPath();
    std::ifstream jsonFile(jobsPath);
    if (!jsonFile.is_open()) {
        printError("Cannot open jobs.json configuration file: " + jobsPath);
        printInfo("Make sure the file exists and you have read permissions.");
        return;
    }

    // Read entire configuration file into memory
    std::stringstream buffer;
    buffer << jsonFile.rdbuf();
    std::string jsonContent = buffer.str();
    jsonFile.close();
    
    // Display formatted header
    std::cout << "\n" << CYAN << "=== JOB SCHEDULE OVERVIEW ===" << RESET << "\n\n";
    
    size_t jobStart = 0;
    int jobNumber = 1;
    
    /**
     * Simple JSON parsing loop - finds job objects and extracts fields
     * This approach avoids heavy JSON library dependencies while providing
     * sufficient parsing capability for configuration display purposes.
     */
    while ((jobStart = jsonContent.find("{", jobStart)) != std::string::npos) {
        size_t jobEnd = jsonContent.find("}", jobStart);
        if (jobEnd == std::string::npos) break;
        
        std::string jobSection = jsonContent.substr(jobStart, jobEnd - jobStart + 1);
        
        // Skip non-job objects (schedule/conditions sub-objects)
        if (jobSection.find("\"command\"") == std::string::npos) {
            jobStart = jobEnd + 1;
            continue;
        }
        
        std::cout << YELLOW << "Job #" << jobNumber << RESET << "\n";
        std::cout << "----------------------------------------\n";
        
        /**
         * Field extraction lambda - parses JSON field values using string operations
         * Handles quoted string values and basic JSON structure without full parsing
         */
        auto extractField = [&](const std::string& field) -> std::string {
            std::string searchStr = "\"" + field + "\":";
            size_t pos = jobSection.find(searchStr);
            if (pos == std::string::npos) return "";
            
            pos += searchStr.length();
            while (pos < jobSection.length() && (jobSection[pos] == ' ' || jobSection[pos] == '\t')) pos++;
            
            if (pos >= jobSection.length() || jobSection[pos] != '"') return "";
            pos++; // Skip opening quote
            
            size_t endPos = jobSection.find("\"", pos);
            if (endPos == std::string::npos) return "";
            
            return jobSection.substr(pos, endPos - pos);
        };
        
        // Extract basic job information
        std::string command = extractField("command");
        std::string description = extractField("description");
        
        /**
         * Schedule parsing - extracts nested schedule object fields
         * Handles the hierarchical JSON structure for schedule specifications
         */
        size_t scheduleStart = jobSection.find("\"schedule\"");
        std::string minute = "*", hour = "*", day_of_month = "*", month = "*", day_of_week = "*";
        
        if (scheduleStart != std::string::npos) {
            size_t scheduleObjStart = jobSection.find("{", scheduleStart);
            size_t scheduleObjEnd = jobSection.find("}", scheduleObjStart);
            if (scheduleObjStart != std::string::npos && scheduleObjEnd != std::string::npos) {
                std::string scheduleSection = jobSection.substr(scheduleObjStart, scheduleObjEnd - scheduleObjStart);
                
                // Schedule field extraction lambda for nested parsing
                auto extractScheduleField = [&](const std::string& field) -> std::string {
                    std::string searchStr = "\"" + field + "\":";
                    size_t pos = scheduleSection.find(searchStr);
                    if (pos == std::string::npos) return "*";
                    
                    pos += searchStr.length();
                    while (pos < scheduleSection.length() && (scheduleSection[pos] == ' ' || scheduleSection[pos] == '\t')) pos++;
                    
                    if (pos >= scheduleSection.length() || scheduleSection[pos] != '"') return "*";
                    pos++;
                    
                    size_t endPos = scheduleSection.find("\"", pos);
                    if (endPos == std::string::npos) return "*";
                    
                    return scheduleSection.substr(pos, endPos - pos);
                };
                
                // Extract all schedule components
                minute = extractScheduleField("minute");
                hour = extractScheduleField("hour");
                day_of_month = extractScheduleField("day_of_month");
                month = extractScheduleField("month");
                day_of_week = extractScheduleField("day_of_week");
            }
        }
        
        /**
         * Conditions parsing - extracts optional execution conditions
         * Builds human-readable condition descriptions for system resource thresholds
         */
        std::string conditions = "";
        size_t conditionsStart = jobSection.find("\"conditions\"");
        if (conditionsStart != std::string::npos) {
            size_t condObjStart = jobSection.find("{", conditionsStart);
            size_t condObjEnd = jobSection.find("}", condObjStart);
            if (condObjStart != std::string::npos && condObjEnd != std::string::npos) {
                std::string condSection = jobSection.substr(condObjStart, condObjEnd - condObjStart);
                
                // Conditions field extraction lambda
                auto extractCondField = [&](const std::string& field) -> std::string {
                    std::string searchStr = "\"" + field + "\":";
                    size_t pos = condSection.find(searchStr);
                    if (pos == std::string::npos) return "";
                    
                    pos += searchStr.length();
                    while (pos < condSection.length() && (condSection[pos] == ' ' || condSection[pos] == '\t')) pos++;
                    
                    if (pos >= condSection.length() || condSection[pos] != '"') return "";
                    pos++;
                    
                    size_t endPos = condSection.find("\"", pos);
                    if (endPos == std::string::npos) return "";
                    
                    return condSection.substr(pos, endPos - pos);
                };
                
                // Extract system condition thresholds
                std::string cpu = extractCondField("cpu");
                std::string ram = extractCondField("ram");
                std::string loadavg = extractCondField("loadavg");
                
                // Build conditions description
                std::vector<std::string> condList;
                if (!cpu.empty()) condList.push_back("CPU " + cpu);
                if (!ram.empty()) condList.push_back("RAM " + ram);
                if (!loadavg.empty()) condList.push_back("Load " + loadavg);
                
                if (!condList.empty()) {
                    conditions = "Only when: ";
                    for (size_t i = 0; i < condList.size(); i++) {
                        conditions += condList[i];
                        if (i < condList.size() - 1) conditions += ", ";
                    }
                }
            }
        }
        
        // Display formatted job information with color coding
        std::cout << GREEN << "Command: " << RESET << command << "\n";
        std::cout << BLUE << "Description: " << RESET << description << "\n";
        std::cout << MAGENTA << "Schedule: " << RESET << scheduleToText(minute, hour, day_of_month, month, day_of_week) << "\n";
        
        if (!conditions.empty()) {
            std::cout << RED << "Conditions: " << RESET << conditions << "\n";
        }
        
        std::cout << "\n";
        
        jobStart = jobEnd + 1;
        jobNumber++;
    }
    
    if (jobNumber == 1) {
        printWarning("No jobs found in configuration file.");
    }
}

/**
 * @brief Opens jobs.json configuration file in text editor for modification
 * 
 * Provides seamless job configuration editing with automatic editor detection
 * and auto-reload notification. Attempts multiple editors in preference order
 * and informs users about the daemon's auto-reload capability for immediate
 * configuration changes without restart.
 */
void editJobs() {
    printInfo("[editjobs] Opening jobs.json for editing...");
    
    std::string jobsPath = getJobsJsonPath();
    
    // Verify file accessibility before attempting to edit
    std::ifstream testFile(jobsPath);
    if (!testFile.is_open()) {
        printError("Cannot access jobs.json configuration file: " + jobsPath);
        printInfo("Make sure the file exists and you have read permissions.");
        return;
    }
    testFile.close();
    
    // Check daemon status to determine auto-reload availability
    bool daemonRunning = isDaemonRunning();
    
    /**
     * Editor preference order: nano (user-friendly) -> vim/vi (ubiquitous) 
     * -> gedit (GUI) -> code (VS Code). This order prioritizes terminal-based
     * editors for server environments while supporting desktop development.
     */
    std::vector<std::string> editors = {"nano", "vim", "vi", "gedit", "code"};
    
    for (const std::string& editor : editors) {
        // Check editor availability using which command
        std::string checkCmd = "which " + editor + " > /dev/null 2>&1";
        if (system(checkCmd.c_str()) == 0) {
            printInfo("Opening with " + editor + "...");
            
            std::string editCmd = editor + " \"" + jobsPath + "\"";
            int result = system(editCmd.c_str());
            
            if (result == 0) {
                printSuccess("File editing completed.");
                
                /**
                 * Inform user about auto-reload capability based on daemon status
                 * This provides immediate feedback about whether changes will be
                 * automatically applied or require manual daemon restart.
                 */
                if (daemonRunning) {
                    printInfo("Configuration will be automatically reloaded by the daemon!");
                } else {
                    printWarning("Daemon is not running - changes will take effect when started.");
                    printInfo("Start the daemon with: start");
                }
            } else {
                printError("Error occurred while editing the file.");
            }
            return;
        }
    }
    
    printError("No suitable editor found (tried: nano, vim, vi, gedit, code)");
    printInfo("You can manually edit: " + jobsPath);
}

/**
 * @brief Retrieves and displays comprehensive daemon status information
 * 
 * Performs multi-level status checking including executable presence,
 * process detection, and configuration file validation. Provides detailed
 * feedback for troubleshooting daemon issues and system state verification.
 */
void getStat() {
    printInfo("[getstat] Checking daemon status...");

    // Verify nanoCron executable exists in expected location
    int fileCheck = system("test -f /usr/local/bin/nanoCron");
    if (fileCheck != 0) {
        printWarning("nanoCron executable NOT found in /usr/local/bin/");
        return;
    }

    /**
     * Check daemon process status using enhanced detection
     * Uses the improved getDaemonStatus() function that distinguishes
     * between daemon and CLI processes for accurate status reporting.
     */
    auto [running, pid] = getDaemonStatus();
    if (running) {
        printSuccess("nanoCron daemon is RUNNING with PID: " + std::to_string(pid));
        
        // Display detailed process information excluding CLI instance
        std::string psCmd = "ps aux | grep nanoCron | grep -v nanoCronCLI | grep -v grep | head -1";
        system(psCmd.c_str());
    } else {
        printWarning("nanoCron daemon is NOT running.");
    }

    // Verify configuration file accessibility using resolved path
    std::string jobsPath = getJobsJsonPath();
    std::string testCommand = "test -f \"" + jobsPath + "\"";
    int jsonCheck = system(testCommand.c_str());
    if (jsonCheck == 0) {
        printInfo("Configuration file: " + jobsPath + " found.");
    } else {
        printWarning("Configuration file: " + jobsPath + " NOT found.");
    }
}

/**
 * @brief Displays recent log entries with syntax highlighting
 * @param lines Number of recent log lines to display (default: 20)
 * 
 * Implements colored log output based on log level detection.
 * Provides tail-like functionality with enhanced readability through
 * color coding of different log message types (ERROR, SUCCESS, WARN, etc.).
 */
void getLog(int lines = 20) {
    printInfo("[getlog] Showing last " + std::to_string(lines) + " log entries...");
    
    // Use resolved log path from configuration
    std::string logPath = getCronLogPath();
    std::ifstream logFile(logPath);
    if (!logFile.is_open()) {
        printError("Cannot open log file: " + logPath);
        printInfo("Make sure the file exists and you have read permissions.");
        return;
    }

    // Read all log lines into memory for tail-like processing
    std::vector<std::string> logLines;
    std::string line;
    
    while (std::getline(logFile, line)) {
        logLines.push_back(line);
    }
    logFile.close();

    /**
     * Display last N lines with color coding based on log level
     * Color mapping: ERROR (red), SUCCESS (green), WARN (yellow),
     * DEBUG (blue), INFO (cyan), default (no color)
     */
    int start = std::max(0, (int)logLines.size() - lines);
    for (int i = start; i < (int)logLines.size(); i++) {
        const std::string& logLine = logLines[i];
        
        // Log level detection and color application
        if (logLine.find("[ERROR]") != std::string::npos) {
            std::cout << RED << logLine << RESET << std::endl;
        } else if (logLine.find("[SUCCESS]") != std::string::npos) {
            std::cout << GREEN << logLine << RESET << std::endl;
        } else if (logLine.find("[WARN]") != std::string::npos) {
            std::cout << YELLOW << logLine << RESET << std::endl;
        } else if (logLine.find("[DEBUG]") != std::string::npos) {
            std::cout << BLUE << logLine << RESET << std::endl;
        } else if (logLine.find("[INFO]") != std::string::npos) {
            std::cout << CYAN << logLine << RESET << std::endl;
        } else {
            std::cout << logLine << std::endl;
        }
    }
    
    printInfo("Log loaded from: " + logPath);
}

/**
 * @brief Enhanced daemon startup function with comprehensive error detection
 * 
 * Implements robust daemon starting with pre-flight checks and detailed
 * error reporting. Verifies executable presence, permissions, and provides
 * specific troubleshooting guidance for common startup failures.
 */
void startDaemon() {
    printInfo("[start] Starting nanoCron daemon...");
    
    // Check if daemon is already running to prevent duplicate instances
    if (isDaemonRunning()) {
        printWarning("nanoCron daemon is already running.");
        auto [running, pid] = getDaemonStatus();
        if (running) {
            printInfo("Current daemon PID: " + std::to_string(pid));
        }
        return;
    }

    // Pre-flight checks for successful daemon startup
    int fileCheck = system("test -f /usr/local/bin/nanoCron");
    if (fileCheck != 0) {
        printError("nanoCron executable not found. Please compile and install first.");
        return;
    }

    // Verify executable permissions
    int readCheck = system("test -r /usr/local/bin/nanoCron");
    if (readCheck != 0) {
        printError("Cannot read nanoCron executable. Permission issue.");
        printInfo("Try: sudo chmod +x /usr/local/bin/nanoCron");
        return;
    }

    printInfo("Starting daemon in background...");
    
    /**
     * Start daemon using nohup for background execution with output redirection
     * This ensures the daemon survives terminal session termination and
     * doesn't interfere with CLI operation through stdout/stderr.
     */
    int startStatus = system("nohup /usr/local/bin/nanoCron > /dev/null 2>&1 &");
    
    if (startStatus == 0) {
        // Allow time for daemon initialization and startup procedures
        printInfo("Waiting for daemon to initialize...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Verify successful startup by checking process status
        auto [running, pid] = getDaemonStatus();
        if (running) {
            printSuccess("nanoCron daemon started successfully with PID: " + std::to_string(pid));
        } else {
            printError("Daemon startup failed or crashed immediately.");
            printInfo("Check the log file for errors:");
            printInfo("  > getlog");
        }
    } else {
        printError("Failed to execute nanoCron daemon.");
        printInfo("Check if the executable exists and has proper permissions:");
        printInfo("  ls -la /usr/local/bin/nanoCron");
    }
}

/**
 * @brief Enhanced daemon shutdown function with systemd integration
 * 
 * Implements graceful daemon termination with systemd service detection
 * and fallback to manual process termination. Uses escalating signal
 * approach (SIGTERM -> SIGKILL) for reliable shutdown.
 */
void stopDaemon() {
    printInfo("[stop] Stopping nanoCron daemon...");

    /**
     * Check for systemd service management first
     * If daemon is managed by systemd, use systemctl for proper service
     * lifecycle management instead of direct process signals.
     */
    int isActive = system("systemctl is-active --quiet nanoCron.service 2>/dev/null");
    if (isActive == 0) {
        printInfo("Detected systemd service: stopping via systemctl...");
        int stopResult = system("sudo systemctl stop nanoCron.service");
        if (stopResult == 0) {
            printSuccess("nanoCron service stopped via systemctl.");
        } else {
            printError("Failed to stop nanoCron via systemctl.");
        }
        return;
    }

    // Fallback to manual process termination for non-systemd environments
    auto [running, pid] = getDaemonStatus();
    if (!running) {
        printWarning("nanoCron daemon is not running.");
        return;
    }

    printInfo("nanoCron daemon found with PID " + std::to_string(pid) + ". Stopping it...");

    /**
     * Graceful shutdown sequence using escalating signals
     * 1. SIGTERM (graceful) - allows daemon to clean up resources
     * 2. Wait for voluntary termination 
     * 3. SIGKILL (forced) - if graceful shutdown fails
     */
    std::string cmd = "kill -TERM " + std::to_string(pid);
    int result = system(cmd.c_str());
    
    // Allow time for graceful shutdown processing
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auto [stillRunning, newPid] = getDaemonStatus();
    if (!stillRunning) {
        printSuccess("nanoCron daemon stopped successfully.");
    } else {
        printInfo("Daemon still running, trying SIGKILL...");
        cmd = "kill -KILL " + std::to_string(pid);
        system(cmd.c_str());
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Final verification of termination
        auto [finalCheck, finalPid] = getDaemonStatus();
        if (!finalCheck) {
            printSuccess("nanoCron daemon force-stopped successfully.");
        } else {
            printError("Failed to stop nanoCron daemon. Try with sudo or check permissions.");
        }
    }
}

/**
 * @brief Verifies auto-reload functionality through log analysis
 * 
 * Examines recent log entries to confirm ConfigWatcher operation and
 * auto-reload capability. Provides diagnostic information about the
 * inotify-based configuration monitoring system.
 */
void checkAutoReload() {
    printInfo("[checkreload] Verifying auto-reload functionality...");
    
    // Prerequisite: Daemon must be running for auto-reload to function
    auto [running, pid] = getDaemonStatus();
    if (!running) {
        printWarning("Daemon is not running - auto-reload not available.");
        printInfo("Start the daemon first with: start");
        return;
    }
    
    printSuccess("✅ Daemon is running (PID: " + std::to_string(pid) + ")");
    
    /**
     * Log analysis for ConfigWatcher activity
     * Searches recent log entries for ConfigWatcher initialization and
     * reload event messages to verify auto-reload system operation.
     */
    std::string logPath = getCronLogPath();
    std::ifstream logFile(logPath);
    if (!logFile.is_open()) {
        printWarning("Cannot access log file to verify auto-reload status.");
        return;
    }
    
    std::vector<std::string> recentLines;
    std::string line;
    
    // Read all log lines for analysis
    while (std::getline(logFile, line)) {
        recentLines.push_back(line);
    }
    logFile.close();
    
    /**
     * Analyze last 50 log lines for ConfigWatcher indicators
     * Looks for initialization messages and reload events to determine
     * auto-reload system health and activity.
     */
    bool autoReloadEnabled = false;
    bool hasReloadEvents = false;
    int reloadCount = 0;
    
    int start = std::max(0, (int)recentLines.size() - 50);
    for (int i = start; i < (int)recentLines.size(); i++) {
        const std::string& logLine = recentLines[i];
        
        // Detection patterns for auto-reload system status
        if (logLine.find("Configuration auto-reload enabled") != std::string::npos) {
            autoReloadEnabled = true;
        }
        if (logLine.find("ConfigWatcher: Started watching") != std::string::npos) {
            autoReloadEnabled = true;
        }
        if (logLine.find("Configuration file changed, reloading") != std::string::npos ||
            logLine.find("Successfully reloaded") != std::string::npos) {
            hasReloadEvents = true;
            reloadCount++;
        }
    }
    
    /**
     * Report auto-reload system status with actionable recommendations
     * Provides clear feedback about system state and suggests next steps
     * for testing or troubleshooting auto-reload functionality.
     */
    if (autoReloadEnabled) {
        printSuccess("Auto-reload is ENABLED and monitoring configuration file");
        
        if (hasReloadEvents) {
            printInfo("Found " + std::to_string(reloadCount) + " recent reload event(s)");
            printSuccess("Auto-reload is working correctly!");
        } else {
            printInfo("No recent reload events (configuration hasn't changed recently)");
            printInfo("Try editing jobs.json to test auto-reload:");
            printInfo(" > editjobs");
        }
    } else {
        printWarning(" Auto-reload status unclear from recent logs");
        printInfo("Try restarting the daemon to ensure auto-reload is enabled:");
        printInfo(" > restart");
    }
    
    printInfo("View full logs with: getlog");
}

/**
 * @brief Enhanced daemon restart function with state verification
 * 
 * Performs complete daemon restart cycle with intermediate state checking
 * to ensure clean shutdown before attempting restart. Prevents issues
 * from incomplete termination affecting new daemon instance.
 */
void restartDaemon() {
    printInfo("[restart] Restarting nanoCron daemon...");
    
    // Record initial daemon state for informational purposes
    auto [wasRunning, oldPid] = getDaemonStatus();
    if (wasRunning) {
        printInfo("Stopping current daemon (PID: " + std::to_string(oldPid) + ")...");
    }
    
    stopDaemon();
    
    /**
     * Extended waiting period for complete shutdown
     * Ensures all daemon resources are properly released and background
     * threads are terminated before attempting to start new instance.
     */
    printInfo("Waiting for complete shutdown...");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Verify complete termination before restart attempt
    if (isDaemonRunning()) {
        printError("Previous daemon instance is still running. Cannot restart.");
        printInfo("Please stop it manually and try again:");
        printInfo("  sudo pkill -f nanoCron");
        return;
    }
    
    startDaemon();
}

/**
 * @brief Easter egg function displaying ASCII robot art
 * 
 * Provides a fun interactive element with animated ASCII art display.
 * Demonstrates the animated art functionality while adding personality
 * to the CLI tool.
 */
void nano() {
    std::string nanoascii = R"(
                __
             .-'  |
            /   <\|
           /     \'
           |_.- o-o
           / C  -._)\
          /',        |
         |   `-,_,__,'
         (,,)====[_]=|
           '.   ____/
            | -|-|_
            |____)_)
    )";
    printAsciiArtGradually(nanoascii, 5);
}

/**
 * @brief Main CLI entry point and command processing loop
 * @return Exit code (0 for normal termination)
 * 
 * Initializes the interactive CLI environment with ASCII art banner
 * and enters the main command processing loop. Handles user input
 * parsing and dispatches to appropriate command functions.
 */
int main() {
    // ASCII art banner for visual appeal and branding
    std::string ascii = R"(
    _   __                  ______               
   / | / /___ _____  ____  / ____/________  ____ 
  /  |/ / __ `/ __ \/ __ \/ /   / ___/ __ \/ __ \           Author: Giuseppe Puleri
 / /|  / /_/ / / / / /_/ / /___/ /  / /_/ / / / /           License:  BSD 2-clause
/_/ |_/\__,_/_/ /_/\____/\____/_/   \____/_/ /_/            For: Linux systems
v: 2.0.0 - Interactive CLI

    )";

    // Display banner with typewriter effect
    printAsciiArtGradually(ascii, 3);

    printInfo("nanoCron Interactive CLI. Type 'help' for commands.");

    std::string cmd;
    /**
     * Main command processing loop
     * Continues until user exits, processing commands and providing
     * appropriate responses with error handling for unknown commands.
     */
    while (true) {
        std::cout << CYAN << "> " << RESET;
        std::getline(std::cin, cmd);

        // Command dispatch with aliases and parameter handling
        if (cmd == "getstat" || cmd == "status") {
            getStat();
        } else if (cmd == "getlog" || cmd == "log") {
            getLog();
        } else if (cmd.find("getlog ") == 0) {
            // Extract number of lines parameter for log display
            try {
                int lines = std::stoi(cmd.substr(7));
                getLog(lines);
            } catch (const std::exception&) {
                printError("Invalid number format. Usage: getlog [number]");
            }
        } else if (cmd == "start") {
            startDaemon();
        } else if (cmd == "stop") {
            stopDaemon();
        } else if (cmd == "restart") {
            restartDaemon();
        } else if (cmd == "seejobs") {
            seeJobs();
        } else if (cmd == "editjobs") {
            editJobs();
        } else if (cmd == "checkreload") {
            checkAutoReload();
        } else if (cmd == "exit" || cmd == "quit") {
            printInfo("Goodbye! nanoCron daemon continues running in background.");
            break;
        } else if (cmd == "help"|| cmd == "h") {
            // Display comprehensive help information
            printInfo("Available commands:");
            std::cout << YELLOW << " getstat          " << RESET << "               - Show daemon status\n";
            std::cout << YELLOW << " getlog           " << RESET << "               - Show recent log entries (default: 20)\n";
            std::cout << YELLOW << " start            " << RESET << "               - Start the daemon\n";
            std::cout << YELLOW << " stop             " << RESET << "               - Stop the daemon\n";
            std::cout << YELLOW << " restart          " << RESET << "               - Restart the daemon\n";
            std::cout << YELLOW << " seejobs          " << RESET << "               - Show jobs in readable format\n";
            std::cout << YELLOW << " editjobs         " << RESET << "               - Edit jobs configuration (auto-reload!)\n";
            std::cout << YELLOW << " checkreload      " << RESET << "               - Verify auto-reload functionality\n";
            std::cout << YELLOW << " exit/quit        " << RESET << "               - Exit CLI (daemon keeps running)\n";
            std::cout << "\n" << CYAN << "Auto-reload: Configuration changes are detected automatically!" << RESET << "\n";
        } else if (cmd.empty()) {
            // Ignore empty input for better user experience
        } else if (cmd == "nano") {
            nano();  // Easter egg
        } else {
            printWarning("Unknown command: '" + cmd + "'. Type 'help' for available commands.");
        }
    }
    return 0;
}