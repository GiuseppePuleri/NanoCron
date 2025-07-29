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

# ------------------------------------------------------------------------------
# Compilation Step:
# Compile `nanoCron.cpp` located one directory up, placing the executable
# in `/usr/local/bin` so it's globally accessible.
echo "[nanoCron] Compiling nanoCron.cpp..."
#g++ ../nanoCron.cpp -o /usr/local/bin/nanoCron
g++ -O2 -I../components ../nanoCron.cpp ../components/*.cpp -o /usr/local/bin/nanoCron
# Aggiungi qui:
echo "[nanoCron] Compiling nanoCronCLI..."
g++ -O2 ../nanoCronCLI.cpp -o /usr/local/bin/nanoCronCLI

# ------------------------------------------------------------------------------
# Create Configuration Directory:
# Make sure the config folder exists, similar to `/opt/nanoCron/init/`.
# The `-p` flag creates parent directories if they don't exist
# and does nothing if they already exist.
echo "[nanoCron] Creating /opt/nanoCron/init/ if it doesn't exist..."
mkdir -p /opt/nanoCron/init

# ------------------------------------------------------------------------------
# Copy Configuration File:
# Place your `config.env` into the config directory so systemd can read it.
echo "[nanoCron] Copying config.env..."
cp ./config.env /opt/nanoCron/init/config.env

# ------------------------------------------------------------------------------
# Log File Setup:
# Ensure the log file exists and set permissions so both owner and group can write.
echo "[nanoCron] Creating log file if needed..."
touch /var/log/nanoCron.log
chmod 664 /var/log/nanoCron.log

# ------------------------------------------------------------------------------
# Install Systemd Service:
# Copy the service unit file to systemd’s directory so it can manage the daemon.
echo "[nanoCron] Installing systemd service..."
cp ./nanoCron.service /etc/systemd/system/nanoCron.service

# ------------------------------------------------------------------------------
# Enable and Start Service:
# Reload systemd configuration, then enable (auto-start on boot) and start nanoCron.
echo "[nanoCron] Reloading systemd config and starting service..."
systemctl daemon-reexec
systemctl daemon-reload
systemctl enable nanoCron
systemctl restart nanoCron

# ------------------------------------------------------------------------------
echo "[nanoCron] Installation complete! Service is now active."
