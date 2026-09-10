/**
 * @file CronEngine.cpp
 * @brief Core scheduling logic: decides whether a job should run right now.
 */

#include "CronEngine.h"
#include <sstream>

bool CronEngine::shouldRunJob(const CronJob& job,
                             const std::tm& local_time,
                             const std::map<std::string, std::pair<int, int>>& last_exec) {

    // Resolved-value convention (see CronTypes.h):
    //   -1 = "*" (any value), -2 = "*/N" (interval), >=0 = exact match.
    bool minute_match = (job.minute == -1) ||
                       (job.minute == local_time.tm_min) ||
                       (job.minute == -2 && job.minute_interval > 0 &&
                        local_time.tm_min % job.minute_interval == 0);

    bool hour_match = (job.hour == -1) ||
                     (job.hour == local_time.tm_hour) ||
                     (job.hour == -2 && job.hour_interval > 0 &&
                      local_time.tm_hour % job.hour_interval == 0);

    if (!minute_match || !hour_match) {
        return false;
    }

    // Prevent re-running the same job twice within the same minute
    // (the daemon polls every 20 seconds, i.e. up to 3 times a minute).
    auto it = last_exec.find(job.command);
    if (it != last_exec.end() &&
        it->second.first == local_time.tm_hour &&
        it->second.second == local_time.tm_min) {
        return false;
    }

    switch (job.frequency) {
        case CronFrequency::DAILY:
            return true;

        case CronFrequency::WEEKLY:
            return local_time.tm_wday == job.day_param;

        case CronFrequency::MONTHLY:
            return local_time.tm_mday == job.day_param;

        case CronFrequency::YEARLY:
            return local_time.tm_mday == job.day_param &&
                   (local_time.tm_mon + 1) == job.month_param;

        case CronFrequency::WEEKDAY:
            return local_time.tm_wday >= 1 && local_time.tm_wday <= 5;

        case CronFrequency::WEEKEND:
            return local_time.tm_wday == 0 || local_time.tm_wday == 6;

        default:
            return false;
    }
}

void CronEngine::printJobSchedule(const CronJob& job, Logger& logger) {
    std::stringstream ss;
    ss << "Job: " << job.command << " (" << job.description << ")\n";

    ss << "  Time: ";
    if (job.hour == -1) ss << "*";
    else if (job.hour == -2) ss << "*/" << job.hour_interval;
    else ss << job.hour;
    ss << ":";
    if (job.minute == -1) ss << "*";
    else if (job.minute == -2) ss << "*/" << job.minute_interval;
    else ss << (job.minute < 10 ? "0" : "") << job.minute;
    ss << "\n";

    switch (job.frequency) {
        case CronFrequency::DAILY:
            ss << "  Frequency: Every day";
            break;
        case CronFrequency::WEEKLY:
            ss << "  Frequency: Every " << getWeekdayName(job.day_param);
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

void CronEngine::logSystemStatus(const std::tm& local_time, Logger& logger) {
    std::stringstream ss;
    ss << "Current time: " << local_time.tm_hour << ":"
       << (local_time.tm_min < 10 ? "0" : "") << local_time.tm_min
       << " - " << getWeekdayName(local_time.tm_wday)
       << " " << local_time.tm_mday << "/" << (local_time.tm_mon + 1)
       << "/" << (local_time.tm_year + 1900) << " - System running normally";
    logger.debug(ss.str());
}

std::string CronEngine::getWeekdayName(int wday) {
    static const std::string days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                        "Thursday", "Friday", "Saturday"};
    if (wday < 0 || wday > 6) return "Unknown";
    return days[wday];
}