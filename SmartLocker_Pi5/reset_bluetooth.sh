#!/bin/bash
#
# BLUETOOTH DIAGNOSTIC AND RESET SCRIPT FOR RASPBERRY PI
# Run this to clean up any Bluetooth issues and verify your setup
#
# Usage: sudo ./reset_bluetooth.sh
#

set -e

echo "=========================================="
echo "RASPBERRY PI BLUETOOTH RESET & DIAGNOSTIC"
echo "=========================================="
echo ""

# Check if root
if [ "$EUID" -ne 0 ]; then 
    echo "❌ Please run as root: sudo ./reset_bluetooth.sh"
    exit 1
fi

echo "Step 1: Stopping Bluetooth service..."
systemctl stop bluetooth
sleep 1
echo "✓ Stopped"
echo ""

echo "Step 2: Clearing paired devices..."
# Remove all paired devices
if [ -d /var/lib/bluetooth ]; then
    find /var/lib/bluetooth -name "cache" -type f -delete 2>/dev/null || true
    echo "✓ Cleared device cache"
else
    echo "⚠ No Bluetooth data directory found"
fi
echo ""

echo "Step 3: Resetting Bluetooth configuration..."
# Remove any custom configs
rm -f /etc/bluetooth/main.conf.d/* 2>/dev/null || true

# Create fresh config
mkdir -p /etc/bluetooth/main.conf.d
cat > /etc/bluetooth/main.conf.d/test.conf <<EOF
[General]
DiscoverableTimeout = 0
Discoverable = true
Pairable = true

[Policy]
AutoEnable = true
EOF
echo "✓ Fresh config created"
echo ""

echo "Step 4: Starting Bluetooth service..."
systemctl start bluetooth
sleep 2
echo "✓ Started"
echo ""

echo "Step 5: Checking Bluetooth status..."
if systemctl is-active --quiet bluetooth; then
    echo "✓ Bluetooth service is running"
else
    echo "❌ Bluetooth service is NOT running"
    echo "Try: sudo systemctl status bluetooth"
    exit 1
fi
echo ""

echo "Step 6: Enabling Bluetooth controller..."
# Use bluetoothctl to configure
timeout 5 bluetoothctl <<EOF 2>/dev/null || true
power on
discoverable on
pairable on
agent on
default-agent
exit
EOF
echo "✓ Controller configured"
echo ""

echo "Step 7: Getting your MAC address..."
MAC=$(hciconfig hci0 | grep -oP 'BD Address: \K[A-F0-9:]+' 2>/dev/null || echo "UNKNOWN")
if [ "$MAC" != "UNKNOWN" ]; then
    echo "✓ Your Pi's MAC address is: $MAC"
else
    echo "❌ Could not detect MAC address"
    echo "Try running: hciconfig"
fi
echo ""

echo "Step 8: Testing Bluetooth is responsive..."
if timeout 3 hcitool dev >/dev/null 2>&1; then
    echo "✓ Bluetooth is responsive"
else
    echo "⚠ Bluetooth may not be responding"
fi
echo ""

echo "=========================================="
echo "✓ BLUETOOTH RESET COMPLETE"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Copy this MAC address: $MAC"
echo "2. Put it in TTGO_BT_Test.ino"
echo "3. Run: sudo python3 pi_test_server.py"
echo "4. Upload TTGO_BT_Test.ino to your ESP32"
echo ""
echo "If connection still fails:"
echo "- Restart your Pi (sudo reboot)"
echo "- Check Pi and TTGO are within 5 meters"
echo "- Make sure no other devices are connected"
echo ""
