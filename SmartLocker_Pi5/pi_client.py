#!/usr/bin/env python3
"""
PI BLUETOOTH CLIENT - ULTRA SIMPLE
Usage: python3 pi_client.py AA:BB:CC:DD:EE:FF
"""

import bluetooth
import sys
import time

if len(sys.argv) < 2:
    print("\nUsage: python3 pi_client.py <TTGO_MAC_ADDRESS>")
    print("Example: python3 pi_client.py AA:BB:CC:DD:EE:FF\n")
    sys.exit(1)

ttgo_mac = sys.argv[1].upper()

print("\n========================================")
print("PI BLUETOOTH CLIENT")
print("========================================\n")
print(f"Connecting to TTGO at: {ttgo_mac}\n")

# Connect
print("Attempting connection...")
sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)

try:
    sock.connect((ttgo_mac, 1))
    print("✓✓✓ CONNECTED! ✓✓✓\n")
    
    # Send messages
    count = 0
    while True:
        count += 1
        msg = f"HELLO #{count}"
        
        # Send
        sock.send(msg.encode() + b'\n')
        print(f"→ Sent: {msg}")
        
        # Receive
        response = sock.recv(1024).decode().strip()
        print(f"← TTGO: {response}\n")
        
        time.sleep(3)
        
except KeyboardInterrupt:
    print("\nStopped by user")
except Exception as e:
    print(f"\n✗ Error: {e}")
    print("\nTroubleshooting:")
    print("  - Is TTGO running ttgo_server.ino?")
    print("  - Is MAC address correct?")
    print("  - Are devices close together?")
finally:
    sock.close()
    print("Disconnected\n")
