#ifndef CONFIG_WATCHER_H
#define CONFIG_WATCHER_H

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>
#include <limits.h>
#include "CronTypes.h"
#include "Logger.h"

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

/**
 * ConfigWatcher - Watches jobs.json for changes and hot-reloads it.
 *
 * We watch the *parent directory* of the config file rather than the file
 * itself. Many editors (vim, VSCode, ...) save by writing a temp file and
 * renaming it over the original for atomicity; that replaces the inode,
 * which makes an inotify watch placed directly on the file stop receiving
 * events after the very first save. Watching the directory and filtering
 * events by filename survives that pattern.
 */
class ConfigWatcher {
private:
    std::shared_ptr<std::vector<CronJob>> currentJobs;
    mutable std::mutex cacheMutex;
    std::string configPath;      // Full path to jobs.json, as given by the caller
    std::string watchDir;        // Parent directory of configPath (what inotify actually watches)
    std::string watchedFilename; // Filename portion of configPath (used to filter events)
    Logger& logger;

    int inotifyFd;
    int watchDescriptor;
    std::atomic<bool> isWatching{false};
    std::atomic<bool> shouldStop{false};
    std::thread watcherThread;

    bool initializeInotify();
    void cleanupInotify();
    void watcherLoop();
    bool validateAndLoadConfig();
    std::shared_ptr<std::vector<CronJob>> loadJobsFromFile();

public:
    ConfigWatcher(const std::string& jobsPath, Logger& loggerRef);
    ~ConfigWatcher();

    bool startWatching();
    void stopWatching();
    std::shared_ptr<std::vector<CronJob>> getJobs();
    bool isConfigValid() const;

    // Force an immediate reload, bypassing file-change detection (useful for testing).
    bool forceReload();
};

#endif // CONFIG_WATCHER_H