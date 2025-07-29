#include <iostream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>

// ANSI color codes for colored terminal output
const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string CYAN = "\033[36m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";

// Prints a message in green color indicating success
void printSuccess(const std::string& msg) {
    std::cout << GREEN << msg << RESET << "\n";
}

// Prints a message in red color indicating an error
void printError(const std::string& msg) {
    std::cerr << RED << msg << RESET << "\n";
}

// Prints a message in yellow color indicating a warning
void printWarning(const std::string& msg) {
    std::cout << YELLOW << msg << RESET << "\n";
}

// Prints a message in cyan color indicating informational text
void printInfo(const std::string& msg) {
    std::cout << CYAN << msg << RESET << "\n";
}

// Prints ASCII art gradually character-by-character with a delay
void printAsciiArtGradually(const std::string& art, int delayMs = 50) {
    for (char c : art) {
        std::cout << c << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    std::cout << std::endl;
}

// Checks if the nanoCron daemon is running
bool isDaemonRunning() {
    int status = system("pgrep -f nanoCron > /dev/null 2>&1");
    return (status == 0);
}

// Retrieves and displays the daemon status
void getStat() {
    printInfo("[getstat] Checking daemon status...");

    // Check if nanoCron executable exists
    int fileCheck = system("test -f /usr/local/bin/nanoCron");
    if (fileCheck != 0) {
        printWarning("nanoCron executable NOT found in /usr/local/bin/");
        return;
    }

    // Check if daemon is running
    if (isDaemonRunning()) {
        printSuccess("nanoCron daemon is RUNNING.");
        
        // Show process info
        system("ps aux | grep nanoCron | grep -v grep | head -1");
    } else {
        printWarning("nanoCron daemon is NOT running.");
    }

    // Check if jobs.json exists
    int jsonCheck = system("test -f jobs.json");
    if (jsonCheck == 0) {
        printInfo("Configuration file: jobs.json found.");
    } else {
        printWarning("Configuration file: jobs.json NOT found.");
    }
}

// Shows recent log entries with colored output
void getLog(int lines = 20) {
    printInfo("[getlog] Showing last " + std::to_string(lines) + " log entries...");
    
    std::ifstream logFile("logs/cron.log");
    if (!logFile.is_open()) {
        printError("Cannot open log file: logs/cron.log");
        return;
    }

    std::vector<std::string> logLines;
    std::string line;
    
    // Read all lines
    while (std::getline(logFile, line)) {
        logLines.push_back(line);
    }
    logFile.close();

    // Show last N lines with color coding
    int start = std::max(0, (int)logLines.size() - lines);
    for (int i = start; i < (int)logLines.size(); i++) {
        const std::string& logLine = logLines[i];
        
        // Color code based on log level
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
}

// Starts the nanoCron daemon
void startDaemon() {
    printInfo("[start] Starting nanoCron daemon...");
    
    if (isDaemonRunning()) {
        printWarning("nanoCron daemon is already running.");
        return;
    }

    // Check if executable exists
    int fileCheck = system("test -f /usr/local/bin/nanoCron");
    if (fileCheck != 0) {
        printError("nanoCron executable not found. Please compile and install first.");
        return;
    }

    // Start daemon in background
    int startStatus = system("nohup /usr/local/bin/nanoCron > /dev/null 2>&1 &");
    if (startStatus == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Wait for startup
        if (isDaemonRunning()) {
            printSuccess("nanoCron daemon started successfully.");
        } else {
            printError("Failed to start nanoCron daemon.");
        }
    } else {
        printError("Failed to start nanoCron daemon.");
    }
}

// Stops the nanoCron daemon
void stopDaemon() {
    printInfo("[stop] Stopping nanoCron daemon...");
    
    if (!isDaemonRunning()) {
        printWarning("nanoCron daemon is not running.");
        return;
    }

    int stopStatus = system("pkill -f nanoCron");
    if (stopStatus == 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait for shutdown
        if (!isDaemonRunning()) {
            printSuccess("nanoCron daemon stopped successfully.");
        } else {
            printError("Failed to stop nanoCron daemon.");
        }
    } else {
        printError("Failed to stop nanoCron daemon.");
    }
}

// Restarts the daemon
void restartDaemon() {
    printInfo("[restart] Restarting nanoCron daemon...");
    stopDaemon();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    startDaemon();
}

// Shows current job configuration
void showJobs() {
    printInfo("[jobs] Current job configuration:");
    
    std::ifstream jsonFile("jobs.json");
    if (!jsonFile.is_open()) {
        printError("Cannot open jobs.json configuration file.");
        return;
    }

    std::string line;
    int lineNum = 1;
    while (std::getline(jsonFile, line)) {
        std::cout << MAGENTA << std::to_string(lineNum) << ": " << RESET << line << std::endl;
        lineNum++;
    }
    jsonFile.close();
}

int main() {
    // ASCII art banner
    std::string ascii = R"(
    _   __                  ______               
   / | / /___ _____  ____  / ____/________  ____ 
  /  |/ / __ `/ __ \/ __ \/ /   / ___/ __ \/ __ \           Author: Giuseppe Puleri
 / /|  / /_/ / / / / /_/ / /___/ /  / /_/ / / / /           License:  BSD 2-clause
/_/ |_/\__,_/_/ /_/\____/\____/_/   \____/_/ /_/            For: Linux systems
v: 2.0.0 - Interactive CLI

    )";

    // Print ASCII art gradually
    printAsciiArtGradually(ascii, 5);

    printInfo("nanoCron Interactive CLI. Type 'help' for commands.");

    std::string cmd;
    // Main command loop
    while (true) {
        std::cout << CYAN << "> " << RESET;
        std::getline(std::cin, cmd);

        if (cmd == "getstat" || cmd == "status") {
            getStat();
        } else if (cmd == "getlog" || cmd == "log") {
            getLog();
        } else if (cmd.find("getlog ") == 0) {
            // Extract number of lines
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
        } else if (cmd == "jobs") {
            showJobs();
        } else if (cmd == "exit" || cmd == "quit") {
            printInfo("Goodbye! nanoCron daemon continues running in background.");
            break;
        } else if (cmd == "help") {
            printInfo("Available commands:");
            std::cout << YELLOW << " getstat       " << RESET << "                  - Show daemon status\n";
            std::cout << YELLOW << " getlog        " << RESET << "                  - Show recent log entries (default: 20)\n";
            std::cout << YELLOW << " start         " << RESET << "                  - Start the daemon\n";
            std::cout << YELLOW << " stop          " << RESET << "                  - Stop the daemon\n";
            std::cout << YELLOW << " restart       " << RESET << "                  - Restart the daemon\n";
            std::cout << YELLOW << " jobs          " << RESET << "                  - Show current job configuration\n";
            std::cout << YELLOW << " exit/quit     " << RESET << "                  - Exit CLI (daemon keeps running)\n";
        } else if (cmd.empty()) {
            // Do nothing for empty input
        } else {
            printWarning("Unknown command: '" + cmd + "'. Type 'help' for available commands.");
        }
    }
    return 0;
}