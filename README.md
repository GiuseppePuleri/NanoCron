# NanoCron

## Table of Contents

- [Introduction](#introduction)  
- [Key Features](#key-features)  
- [Installation](#installation)  
- [Usage](#usage)  
- [Configuration](#configuration)  
- [Architecture](#architecture)  
- [Components](#components)  
- [Performance Testing](#performance-testing)  
- [API Reference](#api-reference)  
- [Development](#development)  
- [License](#license)  
- [Author](#author)

---

## Introduction

**NanoCron** is modular cron daemon designed specifically for modern Linux systems. It extends the traditional cron concept with real-time configuration reloads, rich logging, system-aware job scheduling, and an interactive CLI — all without requiring daemon restarts.

- JSON configuration: Uses a JSON file for configuration instead of crontab
- JSON in memory: Jobs are loaded once from a file and stored in memory (RAM).
- Modern daemon: Written in C++ with modular architecture
- Auto-reload: Automatically reloads configuration without restart
- Interactive CLI: User-friendly command-line interface with color output
- Advanced logging: Thread-safe logging system with auto-rotation
- System monitoring: Checks system conditions (CPU, RAM, disk)

---

- Website: https://nanocron.puleri.it
- Video Tutorial: https://nanocron.puleri.it/nanocron_video.mp4

---

## Key Features

- **Zero-Downtime Configuration Updates:** Uses Linux’s inotify subsystem to instantly detect and reload job configuration changes without restarting the daemon.  
- **Interactive CLI Interface:** Manage the daemon lifecycle, view and edit jobs, monitor logs and reload status directly from a smart CLI tool (`nanoCronCLI`).  
- **Advanced Scheduling:** Supports standard cron syntax plus system condition thresholds (CPU, RAM, disk usage) to trigger jobs only when system resources allow.  
- **Robust Logging:** Multi-level colored logging (DEBUG, INFO, WARN, ERROR, SUCCESS) with automatic log rotation and filtering.  
- **Thread-Safe Design:** Uses modern C++ move semantics and mutexes for safe concurrent operations.  
- **Flexible Deployment:** Can run standalone or integrate with systemd for service management.  
- **Comprehensive Performance Testing:** Includes benchmark tools comparing NanoCron to traditional cron implementations.


---

## Installation

1. Clone the repository:

```bash
git clone https://github.com/GiuseppePuleri/NanoCron.git && \
cd NanoCron && \
chmod +x init/install.sh && \
sudo ./init/install.sh
```
```bash
nanoCronCLI
```
---

## Usage

### nanoCronCLI

Launch the interactive CLI tool to control the daemon and manage jobs:

```bash
nanoCronCLI
```

#### Common Commands

| Command      | Alias    | Description                                   |
|--------------|----------|-----------------------------------------------|
| `start`      | —        | Start the nanoCron daemon                      |
| `stop`       | —        | Stop the daemon gracefully                     |
| `restart`    | —        | Restart the daemon                             |
| `getstat`    | `status` | Show daemon status and current processes      |
| `getlog [N]` | `log`    | Display last N log entries (default 20)       |
| `seejobs`    | —        | Show current job configuration in readable form |
| `editjobs`   | —        | Open job configuration file in editor          |
| `checkreload`| —        | Verify configuration auto-reload status        |
| `help`       | `h`      | Show help for commands                         |
| `exit`       | `quit`   | Exit CLI (daemon keeps running)                |

- Logs feature color-coded output for better readability.  
- Editors like nano, vim, gedit, or VSCode are auto-detected for `editjobs`.  
- Configuration changes trigger immediate reloads without downtime.

---

## Configuration

NanoCron’s job configuration uses JSON format (`jobs.json`). Each job defines its schedule, command, and optional system resource conditions.

### Example Job Configuration

```json
{
  "jobs": [
    {
      "description": "Nightly backup",
      "command": "/usr/local/bin/backup.sh",
      "schedule": {
        "minute": "0",
        "hour": "2",
        "day_of_month": "*",
        "month": "*",
        "day_of_week": "*"
      },
      "conditions": {
        "cpu": "<80%",
        "ram": "<90%",
        "disk": {
          "/": "<95%"
        }
      }
    }
  ]
}
```

### Schedule Fields

| Field         | Values              | Description                |
|---------------|---------------------|----------------------------|
| `minute`      | 0-59, `*`, `*/N`    | Minute of the hour          |
| `hour`        | 0-23, `*`, `*/N`    | Hour of the day             |
| `day_of_month`| 1-31, `*`           | Day of the month            |
| `month`       | 1-12, `*`           | Month                      |
| `day_of_week` | 0-6, `*`            | Day of the week (0=Sunday)  |

### System Condition Options (Optional)

- `cpu`: e.g. `<80%` or `>50%` — only run if CPU usage matches condition  
- `ram`: e.g. `<90%` — minimum free memory threshold  
- `loadavg`: e.g. `<1.5` — system load average limit  
- `disk`: object with mount points and usage limits, e.g., `{" /": "<95%"}`

---

## Architecture

NanoCron is designed with modular components for maintainability and extensibility.

```
nanoCron/
├── nanoCron.cpp        # Main daemon process
├── nanoCronCLI.cpp     # CLI interface
└── components/
    ├── ConfigWatcher/  # Monitors config changes using inotify
    ├── CronEngine/     # Scheduling and job logic
    ├── JobExecutor/    # Runs jobs in isolated processes
    ├── JobConfig/      # JSON parsing and validation
    ├── Logger/         # Logging system with rotation
    └── CronTypes.h     # Type definitions
```

### Threads

- **Main Thread:** Job scheduling, maintenance, and status reporting  
- **ConfigWatcher Thread:** Watches `jobs.json` and triggers reloads on changes  
- **Signal Handler:** Gracefully handles termination signals (SIGTERM, SIGINT)

---

## Components

### ConfigWatcher

- Uses Linux inotify to detect file changes in `jobs.json`  
- Validates new configs before applying them  
- Ensures atomic job list replacement with thread safety

### CronEngine

- Evaluates cron schedules and system conditions  
- Prevents duplicate job executions within the same time slot  
- Handles periodic maintenance and status logging

### JobExecutor

- Executes jobs in separate child processes  
- Captures stdout/stderr for logging  
- Ensures cleanup and error handling on job completion

### JobConfig

- Efficient JSON parser with move semantics and preallocation  
- Early validation for malformed configs  
- Extracts job data minimizing memory copies

### Logger

- Multi-level (DEBUG, INFO, WARN, ERROR, SUCCESS)  
- Colored terminal output for CLI  
- Automatic daily log rotation with retention policies  
- Thread-safe file writes

---

## Performance Testing

NanoCron includes a dedicated test suite to benchmark parsing speed and resource usage against traditional cron implementations.

### Test Suite Structure

```
tester/
├── TESTER.md                # Test documentation
├── cron_system_tester       # System cron benchmark executable
├── cron_system_tester.cpp   # System cron test source code
├── nanocron_test_runner     # NanoCron benchmark executable
├── nanocron_test_runner.cpp # NanoCron test source code
├── run_performance_tests.sh # Automated test runner
├── test_jobs.json           # Job definitions used in tests
└── test_logs/               # Saved performance logs
```

### Summary of Benchmark Results

| Metric              | System Cron | NanoCron | Notes                       |
|---------------------|-------------|----------|-----------------------------|
| Average Parse Time   | 2.93 ms     | 2.50 ms  | NanoCron ~15% faster parsing|
| Memory Usage        | 192 KB      | 384 KB   | NanoCron uses JSON overhead  |
| File Size           | 785 bytes   | 3164 bytes | Richer metadata              |
| CPU Usage           | ~0%         | ~0%      | Both very efficient          |

### Running Tests

```bash
cd tester/
chmod +x run_performance_tests.sh
./run_performance_tests.sh

# Logs saved in test_logs/performance.log
```

---

## API Reference

Job configuration follows this TypeScript-like interface:

```typescript
interface Job {
  description: string;
  command: string;
  schedule: {
    minute: string;
    hour: string;
    day_of_month: string;
    month: string;
    day_of_week: string;
  };
  conditions?: {
    cpu?: string;
    ram?: string;
    loadavg?: string;
    disk?: { [mountPoint: string]: string };
  };
}
```

---

## Development

- NanoCron is written in modern C++17, emphasizing thread safety and performance.  
- Contributions are welcome via GitHub pull requests.  
- Build tools and instructions are included in the repository.

---

## License

NanoCron is released under the BSD 2-Clause License.
---

## Author

**Giuseppe Puleri**  

Designed for Linux systems with modern C++ features.  
Focused on operational reliability, usability, and performance.

Special thanks to Niels Lohmann, creator of the nlohmann/json library, for making JSON handling simple and efficient.


## Structure
NanoCron/
├── README.md
├── nanoCron.cpp
├── nanoCronCLI.cpp
├── components/
│   ├── CronEngine.cpp
│   ├── CronEngine.h
│   ├── CronTypes.h
│   ├── JobConfig.cpp
│   ├── JobConfig.h
│   ├── JobExecutor.cpp
│   ├── JobExecutor.h
│   ├── Logger.cpp
│   ├── Logger.h
│   └── json.hpp
├── init/
│   ├── config.env
│   ├── install.sh
│   ├── jobs.json
│   └── jobs/
│       └── makeDir
│       └── makeDir.cpp
│   ├── nanoCron.service
│   └── logs/
│       └── cron.log
├── tester/
    ├── TESTER.md                # Test documentation
    ├── cron_system_tester       # System cron benchmark executable
    ├── cron_system_tester.cpp   # System cron test source code
    ├── nanocron_test_runner     # NanoCron benchmark executable
    ├── nanocron_test_runner.cpp # NanoCron test source code
    ├── run_performance_tests.sh # Automated test runner
    ├── test_jobs.json           # Job definitions used in tests
    └── test_logs/               # Saved performance logs

---
