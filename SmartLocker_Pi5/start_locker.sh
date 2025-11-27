#!/bin/bash
# SmartLocker Pi Startup Script

# config
TTGO_MAC="14:2B:2F:A7:94:22"
WEBHOOK_URL="https://script.google.com/macros/s/AKfycbwU7jvZcrGItGfxu3uS4Ux9vrXrL5ne9Lh0TXLLuW8OUCVsh6H6-UAUgRck5Nj89nfssw/exec"
SPP_CHANNEL=1
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_FILE="/var/log/smartlocker.log"

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') $1" | tee -a "$LOG_FILE"
}

cleanup() {
    log "[INFO] Shutting down SmartLocker..."
    
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
    fi
    
    rfcomm release 0 2>/dev/null
    
    log "[INFO] Cleanup complete"
    exit 0
}

if [ "$EUID" -ne 0 ]; then
    echo "Re-running as root..."
    exec sudo "$0" "$@"
fi

trap cleanup SIGINT SIGTERM

log "=========================================="
log "[INFO] SmartLocker Pi Server Starting"
log "[INFO] TTGO MAC: $TTGO_MAC"
log "[INFO] Webhook: Configured"
log "=========================================="

log "[INFO] Setting up Bluetooth..."

rfkill unblock bluetooth 2>/dev/null

systemctl restart bluetooth
sleep 2

btmgmt power on 2>/dev/null

log "[INFO] Bluetooth controller info:"
btmgmt info 2>&1 | head -10 | tee -a "$LOG_FILE"

while true; do
    rfcomm release 0 2>/dev/null
    
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
        SERVER_PID=""
    fi
    
    log "[INFO] Connecting to TTGO at $TTGO_MAC..."
    
    rfcomm connect 0 "$TTGO_MAC" "$SPP_CHANNEL" &
    RFCOMM_PID=$!
    
    WAIT_COUNT=0
    while [ ! -e /dev/rfcomm0 ] && [ $WAIT_COUNT -lt 30 ]; do
        sleep 1
        WAIT_COUNT=$((WAIT_COUNT + 1))
        
        if ! kill -0 "$RFCOMM_PID" 2>/dev/null; then
            log "[WARN] rfcomm connect failed, retrying in 5 seconds..."
            sleep 5
            continue 2
        fi
    done
    
    if [ ! -e /dev/rfcomm0 ]; then
        log "[WARN] Timeout waiting for /dev/rfcomm0"
        kill "$RFCOMM_PID" 2>/dev/null
        sleep 5
        continue
    fi
    
    log "[INFO] Connected! Starting Python server..."
    
    python3 "$SCRIPT_DIR/locker_server.py" "$WEBHOOK_URL" &
    SERVER_PID=$!
    
    log "[INFO] Server started (PID: $SERVER_PID)"
    
    while kill -0 "$RFCOMM_PID" 2>/dev/null && kill -0 "$SERVER_PID" 2>/dev/null; do
        sleep 5
    done
    
    if ! kill -0 "$RFCOMM_PID" 2>/dev/null; then
        log "[WARN] Bluetooth connection lost"
    fi
    
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        log "[WARN] Python server exited"
    fi
    
    log "[INFO] Reconnecting in 3 seconds..."
    sleep 3
done
