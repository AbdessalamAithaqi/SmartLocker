#!/bin/bash
# Setup Bluetooth on Raspberry Pi for Smart Locker
# Run this ONCE to configure your Pi properly

set -e

echo "=========================================="
echo "Smart Locker - Bluetooth Setup"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root: sudo ./setup_bluetooth.sh"
    exit 1
fi

echo "1. Checking Bluetooth service..."
systemctl status bluetooth >/dev/null 2>&1 && echo "✅ Bluetooth service is running" || {
    echo "❌ Bluetooth service not running - starting it..."
    systemctl start bluetooth
    systemctl enable bluetooth
}

echo ""
echo "2. Adding user to bluetooth group..."
USER_NAME=${SUDO_USER:-$USER}
usermod -a -G bluetooth $USER_NAME
echo "✅ User $USER_NAME added to bluetooth group"

echo ""
echo "3. Configuring Bluetooth to be discoverable..."

# Create/update bluetoothd configuration
cat > /etc/bluetooth/main.conf.d/locker.conf <<EOF
[General]
# Make Bluetooth always discoverable and connectable
DiscoverableTimeout = 0
Discoverable = true

[Policy]
# Auto-enable Bluetooth controller on boot
AutoEnable=true
EOF

echo "✅ Bluetooth configuration updated"

echo ""
echo "4. Restarting Bluetooth service..."
systemctl restart bluetooth
sleep 2

echo ""
echo "5. Making adapter discoverable and pairable..."
bluetoothctl <<EOF
power on
discoverable on
pairable on
agent on
default-agent
EOF

echo ""
echo "=========================================="
echo "✅ Bluetooth Setup Complete!"
echo "=========================================="
echo ""
echo "Your Pi is now:"
echo "  - Discoverable"
echo "  - Pairable"
echo "  - Ready for connections"
echo ""
echo "Pi MAC Address:"
hcitool dev | grep -oP 'hci0\s+\K[A-F0-9:]+' || echo "Could not detect MAC"
echo ""
echo "Next steps:"
echo "1. Use this MAC address in your TTGO config.h"
echo "2. Run: python3 pi_server_fixed.py"
echo "3. Power on your TTGO - it should connect!"
echo ""
echo "Note: You may need to log out and back in for group changes to take effect."
echo ""
