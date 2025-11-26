#!/bin/bash
# SmartLocker TTGO Bluetooth autoconnect script
# Call it with:  ./bt_autoconnect.sh
# It will re-exec itself as root if needed.

MAC="14:2B:2F:A7:94:22"   # TTGO MAC address
CHAN=1                    # SPP channel (1 for our sketch)

# --- Make sure we are root (so no sudo spam inside) ---
if [ "$EUID" -ne 0 ]; then
  echo "Re-running as root..."
  exec sudo "$0" "$@"
fi

echo "$(date) -> SmartLocker BT autostart"

# --- Bring up Bluetooth hardware and BlueZ stack ---
echo "$(date) -> Unblocking bluetooth via rfkill"
rfkill unblock bluetooth || true

echo "$(date) -> Restarting bluetooth.service"
systemctl restart bluetooth || true

echo "$(date) -> Powering controller with btmgmt"
btmgmt power on || true

# (Optional) print controller info for debugging
btmgmt info || true

# --- Main reconnect loop ---
while true; do
  # Clean any stale binding
  rfcomm release 0 >/dev/null 2>&1 || true

  echo "$(date) -> Trying to connect to $MAC on channel $CHAN..."
  rfcomm connect 0 "$MAC" "$CHAN"

  echo "$(date) -> rfcomm exited with code $?; retrying in 3 seconds..."
  sleep 3
done

