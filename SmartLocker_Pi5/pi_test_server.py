#!/usr/bin/env python3
"""
MINIMAL BLUETOOTH SERVER FOR RASPBERRY PI
Tests basic Bluetooth connectivity without any Google Sheets stuff

Run this on your Pi:
    sudo python3 pi_test_server.py

Expected behavior:
- Pi advertises Bluetooth service
- Accepts connections from TTGO
- Echoes back any messages received
- Simple, clear debug output
"""

import bluetooth
import sys

print("=" * 60)
print("MINIMAL BLUETOOTH TEST SERVER")
print("=" * 60)
print()

# Step 1: Show Pi's MAC address
print("Getting your Pi's Bluetooth MAC address...")
try:
    devices = bluetooth.discover_devices(lookup_names=False)
    local_addr = bluetooth.read_local_bdaddr()
    print(f"✓ Your Pi's MAC address: {local_addr}")
    print()
    print("IMPORTANT: Use this MAC in your TTGO code!")
    print()
except Exception as e:
    print(f"✗ Could not read MAC address: {e}")
    print("Try running: hciconfig")
    print()

# Step 2: Create RFCOMM server socket
print("Creating Bluetooth server socket...")
try:
    server_sock = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
    print("✓ Socket created")
except Exception as e:
    print(f"✗ Failed to create socket: {e}")
    sys.exit(1)

# Step 3: Bind to port 1
print("Binding to RFCOMM port 1...")
try:
    server_sock.bind(("", 1))
    server_sock.listen(1)
    print("✓ Listening on port 1")
except Exception as e:
    print(f"✗ Failed to bind: {e}")
    print("Make sure no other Bluetooth server is running")
    sys.exit(1)

# Step 4: Advertise service
print("Advertising Bluetooth service...")
try:
    bluetooth.advertise_service(
        server_sock,
        "TTGO_Test_Server",
        service_id="00001101-0000-1000-8000-00805F9B34FB",
        service_classes=["00001101-0000-1000-8000-00805F9B34FB", bluetooth.SERIAL_PORT_CLASS],
        profiles=[bluetooth.SERIAL_PORT_PROFILE],
    )
    print("✓ Service advertised")
except Exception as e:
    print(f"⚠ Service advertisement failed: {e}")
    print("This is OK - client can still connect if it knows the MAC address")

print()
print("=" * 60)
print("SERVER READY - Waiting for TTGO connection...")
print("=" * 60)
print()
print("Now upload and run TTGO_BT_Test.ino on your ESP32")
print()

# Main loop
while True:
    try:
        print("Waiting for connection...")
        client_sock, client_info = server_sock.accept()
        print(f"\n✓✓✓ CONNECTED to {client_info} ✓✓✓\n")
        
        # Handle messages
        while True:
            try:
                data = client_sock.recv(1024)
                if not data:
                    break
                
                message = data.decode('utf-8').strip()
                print(f"← Received: {message}")
                
                # Echo it back
                response = f"OK: {message}\n"
                client_sock.send(response.encode('utf-8'))
                print(f"→ Sent: {response.strip()}")
                
            except bluetooth.BluetoothError as e:
                print(f"✗ Bluetooth error: {e}")
                break
            except Exception as e:
                print(f"✗ Error: {e}")
                break
        
        print("\n✗ Client disconnected\n")
        client_sock.close()
        
    except KeyboardInterrupt:
        print("\n\nShutting down server...")
        break
    except Exception as e:
        print(f"✗ Connection error: {e}")
        print("Waiting for next connection...\n")

server_sock.close()
print("Server stopped")
