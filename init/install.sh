#!/bin/bash
set -e

# ------------------------------------------------------------------------------
# [nanoCron] Starting installation...
echo "[nanoCron] Starting installation..."

# ------------------------------------------------------------------------------
# Permission Check:
# Ensure the script runs with root privileges, because installing files
# into system directories and enabling systemd services requires root.
if [[ $EUID -ne 0 ]]; then
   echo "[Error] You must run this script as root (use sudo)"
   exit 1
fi

# ------------------------------------------------------------------------------
# g++ Compiler Check:
# Verify if g++ is available in the system.
# If missing, update package index and install g++ using apt-get.
if ! command -v g++ &> /dev/null; then
    echo "[nanoCron] g++ not found. Installing..."
    apt-get update && apt-get install -y g++
fi

# Check for required C++ version
echo "[nanoCron] Checking C++ compiler..."
if ! g++ --version &> /dev/null; then
    echo "[Error] g++ compiler not found. Please install g++."
    exit 1
fi

# ------------------------------------------------------------------------------
# Compilation Step:
# Compile `nanoCron.cpp` with all components, including new ConfigWatcher
echo "[nanoCron] Compiling nanoCron.cpp with auto-reload support..."
g++ -O2 -pthread -I../components \
    ../nanoCron.cpp \
    ../components/Logger.cpp \
    ../components/JobConfig.cpp \
    ../components/CronEngine.cpp \
    ../components/JobExecutor.cpp \
    ../components/ConfigWatcher.cpp \
    -o /usr/local/bin/nanoCron

echo "[nanoCron] Compiling nanoCronCLI..."
g++ -O2 ../nanoCronCLI.cpp -o /usr/local/bin/nanoCronCLI

# ------------------------------------------------------------------------------
# Create Configuration Directory:
echo "[nanoCron] Removing existing /opt/nanoCron (if present)..."
sudo rm -rf /opt/nanoCron

echo "[nanoCron] Creating /opt/nanoCron/init/..."
sudo mkdir -p /opt/nanoCron/init

# ------------------------------------------------------------------------------
# Create Clean Configuration File:
echo "[nanoCron] Creating clean config.env with paths..."
INSTALL_DIR="$(cd "$(dirname "$0")" && pwd)"

# Create clean config.env with all paths
cat > ./config.env << EOF
CRON_INTERVAL_SECONDS=60
LOG_PATH=/var/log/nanoCron.log
ORIGINAL_JOBS_JSON_PATH=$INSTALL_DIR/jobs.json
ORIGINAL_CRON_LOG_PATH=$INSTALL_DIR/logs/cron.log
EOF

# Copy to system location
echo "[nanoCron] Copying config.env to /opt/nanoCron/init/..."
cp ./config.env /opt/nanoCron/init/config.env

# Create logs directory if it doesn't exist
echo "[nanoCron] Creating logs directory..."
mkdir -p $INSTALL_DIR/logs

# ------------------------------------------------------------------------------
# Install Systemd Service:
# Copy the service unit file to systemd's directory so it can manage the daemon.
echo "[nanoCron] Installing systemd service..."
cp ./nanoCron.service /etc/systemd/system/nanoCron.service

# ------------------------------------------------------------------------------
# Enable and Start Service:
# Reload systemd configuration, then enable (auto-start on boot) and start nanoCron.
echo "[nanoCron] Reloading systemd config and starting service..."
systemctl daemon-reload
systemctl enable nanoCron
systemctl restart nanoCron

# Give the service a moment to start
sleep 3

# Check if service started successfully
if systemctl is-active --quiet nanoCron; then
    echo "[nanoCron] Installation complete! Service is now active with auto-reload support."
    echo "[nanoCron] You can check status with: nanoCronCLI"
    echo "[nanoCron] Configuration changes to jobs.json will be automatically detected!"
else
    echo "[ERROR] Service failed to start. Check logs with: journalctl -u nanoCron"
    exit 1
fi