#!/bin/bash
# SmartLocker Pi Startup Script
# ==============================
# This script:
# 1. Sets up Bluetooth
# 2. Connects to the TTGO (which runs as BT server)
# 3. Starts the Python server to handle webhook communication
#
# Usage:
#   ./start_locker.sh
#
# To run at boot, add to /etc/rc.local or create a systemd service.

# --- Configuration (HARDCODED) ---
TTGO_MAC="14:2B:2F:A7:94:22"
WEBHOOK_URL="https://script.google.com/macros/s/AKfycbwU7jvZcrGItGfxu3uS4Ux9vrXrL5ne9Lh0TXLLuW8OUCVsh6H6-UAUgRck5Nj89nfssw/exec"
SPP_CHANNEL=1
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_FILE="/var/log/smartlocker.log"

# --- Functions ---
log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') $1" | tee -a "$LOG_FILE"
}

cleanup() {
    log "[INFO] Shutting down SmartLocker..."
    
    # Kill Python server if running
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
    fi
    
    # Release rfcomm
    rfcomm release 0 2>/dev/null
    
    log "[INFO] Cleanup complete"
    exit 0
}

# --- Check root ---
if [ "$EUID" -ne 0 ]; then
    echo "Re-running as root..."
    exec sudo "$0" "$@"
fi

# --- Setup signal handlers ---
trap cleanup SIGINT SIGTERM

log "=========================================="
log "[INFO] SmartLocker Pi Server Starting"
log "[INFO] TTGO MAC: $TTGO_MAC"
log "[INFO] Webhook: Configured"
log "=========================================="

# --- Setup Bluetooth ---
log "[INFO] Setting up Bluetooth..."

# Unblock Bluetooth
rfkill unblock bluetooth 2>/dev/null

# Restart Bluetooth service
systemctl restart bluetooth
sleep 2

# Power on controller
btmgmt power on 2>/dev/null

# Show controller info
log "[INFO] Bluetooth controller info:"
btmgmt info 2>&1 | head -10 | tee -a "$LOG_FILE"

# --- Main Loop ---
while true; do
    # Clean up any stale rfcomm binding
    rfcomm release 0 2>/dev/null
    
    # Kill any existing server
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        SERVER_PID=""
    fi
    
    log "[INFO] Connecting to TTGO at $TTGO_MAC..."
    
    # Try to connect (this blocks until connection or failure)
    # Run in background so we can start the Python server
    rfcomm connect 0 "$TTGO_MAC" "$SPP_CHANNEL" &
    RFCOMM_PID=$!
    
    # Wait for /dev/rfcomm0 to appear
    WAIT_COUNT=0
    while [ ! -e /dev/rfcomm0 ] && [ $WAIT_COUNT -lt 30 ]; do
        sleep 1
        WAIT_COUNT=$((WAIT_COUNT + 1))
        
        # Check if rfcomm failed
        if ! kill -0 "$RFCOMM_PID" 2>/dev/null; then
            log "[WARN] rfcomm connect failed, retrying in 5 seconds..."
            sleep 5
            continue 2  # Continue outer loop
        fi
    done
    
    if [ ! -e /dev/rfcomm0 ]; then
        log "[WARN] Timeout waiting for /dev/rfcomm0"
        kill "$RFCOMM_PID" 2>/dev/null
        sleep 5
        continue
    fi
    
    log "[INFO] Connected! Starting Python server..."
    
    # Start Python server in background
    python3 "$SCRIPT_DIR/locker_server.py" "$WEBHOOK_URL" &
    SERVER_PID=$!
    
    log "[INFO] Server started (PID: $SERVER_PID)"
    
    # Wait for either rfcomm or server to exit
    while kill -0 "$RFCOMM_PID" 2>/dev/null && kill -0 "$SERVER_PID" 2>/dev/null; do
        sleep 5
    done
    
    # Something exited - figure out what happened
    if ! kill -0 "$RFCOMM_PID" 2>/dev/null; then
        log "[WARN] Bluetooth connection lost"
    fi
    
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        log "[WARN] Python server exited"
    fi
    
    log "[INFO] Reconnecting in 3 seconds..."
    sleep 3
done
