/**
 * @file JobExecutor.cpp
 * @brief Job execution with timeout protection and result logging.
 */

#include "JobExecutor.h"
#include <cstdlib>
#include <filesystem>

/**
 * Checks once (and caches the result) whether the `timeout` utility is
 * available on this system. Previously this was inferred from the exit
 * code of the job itself (127 * 256), which is ambiguous: 127 also means
 * "command not found" for the *job's own command*, so a missing script
 * looked identical to a missing `timeout` binary and triggered a pointless
 * second execution attempt.
 */
static bool timeoutCommandAvailable() {
    static const bool available =
        (std::system("command -v timeout > /dev/null 2>&1") == 0);
    return available;
}

void JobExecutor::executeJob(const CronJob& job, Logger& logger) {
    logger.info("Starting job: " + job.command, job.description);

    int result = executeWithTimeout(job.command, 300);

    if (result == -1) {
        // system() itself failed to spawn a shell (fork/exec failure at the OS level).
        logger.error("Job could not be started (shell execution failed)", job.description);
    } else if (result == 0) {
        logger.success("Job completed successfully", job.description);
    } else if (result == 124 * 256) {
        // Exit code 124 is the `timeout` utility's convention for "process was killed due to timeout".
        logger.error("Job timed out after 300 seconds", job.description);
    } else {
        // WEXITSTATUS-style: the actual exit code is in the high byte.
        logger.error("Job failed with exit code " + std::to_string(result / 256), job.description);
    }
}

int JobExecutor::executeWithTimeout(const std::string& command, int timeout_seconds) {
    std::string full_command = command;

    // Resolve "./script" to an absolute path so the command still works
    // regardless of the daemon's current working directory.
    if (command.rfind("./", 0) == 0) {
        try {
            auto current_path = std::filesystem::current_path();
            full_command = current_path.string() + "/" + command.substr(2);
        } catch (const std::exception&) {
            // Fall back to the original command if path resolution fails.
        }
    }

    if (timeoutCommandAvailable()) {
        std::string timed_command = "timeout " + std::to_string(timeout_seconds) + " " + full_command;
        return std::system(timed_command.c_str());
    }

    // No timeout utility available: run without timeout protection rather
    // than failing the job outright. Less safe, but keeps the daemon usable
    // on minimal systems.
    return std::system(full_command.c_str());
}