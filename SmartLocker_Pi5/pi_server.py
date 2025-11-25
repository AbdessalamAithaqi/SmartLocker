#!/usr/bin/env python3
"""
Smart Locker Server for Raspberry Pi (Bookworm Compatible)
Forwards Bluetooth requests to Google Sheets via Apps Script webhook
Works without sdptool - uses modern Bluetooth stack
"""

import time
import logging
import bluetooth
import requests
import json

# CONFIG
BLUETOOTH_PORT = 1
LOG_FILE = "locker.log"

# webhook URL
WEBHOOK_URL = "https://script.google.com/macros/s/AKfycbwU7jvZcrGItGfxu3uS4Ux9vrXrL5ne9Lh0TXLLuW8OUCVsh6H6-UAUgRck5Nj89nfssw/exec"

REQUEST_TIMEOUT = 5

# LOGGING
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(message)s",
    handlers=[logging.FileHandler(LOG_FILE), logging.StreamHandler()],
)

# DATABASE
class WebhookDatabase:
    def __init__(self, webhook_url):
        self.webhook_url = webhook_url
        self.offline = False

        try:
            resp = requests.post(
                webhook_url,
                json={"action": "test", "student_id": "ping"},
                timeout=REQUEST_TIMEOUT,
                allow_redirects=True,
            )
            logging.info("Webhook test: %s", resp.status_code)
        except Exception as e:
            logging.warning(f"Webhook test failed (starting anyway): {e}")

    def can_borrow(self, student_id):
        if self.offline:
            logging.warning("Offline mode - denying borrow")
            return False

        try:
            resp = requests.post(
                self.webhook_url,
                json={"action": "check_borrow", "student_id": student_id},
                timeout=REQUEST_TIMEOUT,
                allow_redirects=True,
            )

            try:
                result = resp.json()
            except ValueError:
                logging.error("Non-JSON response from webhook for %s", student_id)
                return False

            can_borrow = bool(result.get("can_borrow", False))
            logging.info("Check borrow %s: %s", student_id, can_borrow)
            return can_borrow

        except Exception as e:
            logging.error(f"Error checking borrow: {e}")
            self.offline = True
            return False

    def borrow_kit(self, student_id):
        if self.offline:
            logging.warning("Offline mode - not logging borrow")
            return False

        try:
            resp = requests.post(
                self.webhook_url,
                json={"action": "borrow", "student_id": student_id},
                timeout=REQUEST_TIMEOUT,
                allow_redirects=True,
            )
            logging.info("Borrow %s: status %s", student_id, resp.status_code)
            return True

        except Exception as e:
            logging.error(f"Error recording borrow: {e}")
            self.offline = True
            return False

    def return_kit(self, student_id):
        try:
            if not self.offline:
                resp = requests.post(
                    self.webhook_url,
                    json={"action": "return", "student_id": student_id},
                    timeout=REQUEST_TIMEOUT,
                    allow_redirects=True,
                )
                logging.info("Return %s: status %s", student_id, resp.status_code)

            logging.info("%s returned kit", student_id)
            return True

        except Exception as e:
            logging.warning(f"Return logging failed, but allowing: {e}")
            self.offline = True
            return True


# BLUETOOTH SERVER
class BluetoothServer:
    def __init__(self, database):
        self.db = database
        self.socket = None
        self.client = None

    def start(self):
        """Start Bluetooth server using modern Python BlueZ bindings"""
        
        # Create RFCOMM socket
        self.socket = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        
        # Bind to any available port
        self.socket.bind(("", BLUETOOTH_PORT))
        self.socket.listen(1)
        
        # Get the port that was bound
        port = self.socket.getsockname()[1]
        
        # Define service UUID (Serial Port Profile)
        uuid = "00001101-0000-1000-8000-00805F9B34FB"
        
        # Advertise service using Python bluetooth library
        try:
            bluetooth.advertise_service(
                self.socket,
                "SmartLocker",
                service_id=uuid,
                service_classes=[uuid, bluetooth.SERIAL_PORT_CLASS],
                profiles=[bluetooth.SERIAL_PORT_PROFILE],
            )
            logging.info("✅ Service advertised successfully (no sdptool needed)")
        except Exception as e:
            logging.warning(f"Service advertisement failed: {e}")
            logging.info("Server will still work if client knows MAC address")
        
        logging.info(f"📡 Bluetooth server ready on port {port}")
        logging.info("Waiting for locker connection...")

        # Main loop
        while True:
            try:
                # Accept connection
                self.client, address = self.socket.accept()
                logging.info(f"📱 Connected to {address}")

                # Handle messages
                while True:
                    data = self.client.recv(1024)
                    if not data:
                        break

                    message = data.decode("utf-8").strip()
                    if message:
                        response = self.process(message)
                        self.client.send((response + "\n").encode("utf-8"))
                        logging.info("[%s] → %s", message, response)

            except bluetooth.BluetoothError as e:
                logging.error(f"Bluetooth error: {e}")
            except Exception as e:
                logging.error(f"Connection error: {e}")
            finally:
                if self.client:
                    self.client.close()
                    self.client = None
                logging.info("Disconnected - waiting for next connection...")

    def process(self, message):
        """Process message from locker"""
        parts = message.split(",")

        if parts[0] == "RETURN":
            if len(parts) >= 2:
                student_id = parts[1]
                self.db.return_kit(student_id)
                return "OK"
            else:
                return "DENIED"

        elif parts[0] == "BORROW":
            return "OK"

        elif parts[0] == "SECURITY_BREACH":
            logging.warning("SECURITY BREACH detected")
            return "OK"

        else:
            # Student ID for borrow request
            student_id = parts[0]

            if student_id.isdigit() and 4 <= len(student_id) <= 12:
                if self.db.can_borrow(student_id):
                    self.db.borrow_kit(student_id)
                    return "OK"
                else:
                    return "DENIED"
            else:
                logging.warning("Invalid student ID format: %s", message)
                return "DENIED"


# MAIN
if __name__ == "__main__":
    logging.info("=" * 50)
    logging.info("SMART LOCKER SERVER (BOOKWORM EDITION)")
    logging.info("=" * 50)

    # Initialize
    db = WebhookDatabase(WEBHOOK_URL)
    server = BluetoothServer(db)

    # Run
    try:
        server.start()
    except KeyboardInterrupt:
        logging.info("\n👋 Server stopped")
