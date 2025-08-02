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

// Fallback for NAME_MAX if not defined
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

class ConfigWatcher {
private:
    std::shared_ptr<std::vector<CronJob>> currentJobs;
    std::mutex cacheMutex;
    std::string configPath;
    Logger& logger;
    
    // inotify variables
    int inotifyFd;
    int watchDescriptor;
    std::atomic<bool> isWatching{false};
    std::atomic<bool> shouldStop{false};
    std::thread watcherThread;
    
    // Internal methods
    bool initializeInotify();
    void cleanupInotify();
    void watcherLoop();
    bool validateAndLoadConfig();
    std::shared_ptr<std::vector<CronJob>> loadJobsFromFile();
    
public:
    ConfigWatcher(const std::string& jobsPath, Logger& loggerRef);
    ~ConfigWatcher();
    
    // Main interface
    bool startWatching();
    void stopWatching();
    std::shared_ptr<std::vector<CronJob>> getJobs();
    bool isConfigValid() const;
    
    // Force reload (useful for testing)
    bool forceReload();
};

#endif // CONFIG_WATCHER_H