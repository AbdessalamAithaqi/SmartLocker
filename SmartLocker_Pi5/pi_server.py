#!/usr/bin/env python3
"""
Smart Locker Server for Raspberry Pi
Forwards Bluetooth requests to Google Sheets via Apps Script webhook
"""

import time
import logging
import bluetooth
import requests
import json
import subprocess  # for sdptool

# ============================================
# CONFIG
# ============================================
BLUETOOTH_PORT = 1
LOG_FILE = "locker.log"

# !!! IMPORTANT: Replace this with your Apps Script Web App URL !!!
# Deploy your Apps Script as a web app and paste the URL here
WEBHOOK_URL = "https://script.google.com/macros/s/AKfycbwU7jvZcrGItGfxu3uS4Ux9vrXrL5ne9Lh0TXLLuW8OUCVsh6H6-UAUgRck5Nj89nfssw/exec"

# Timeout for webhook requests
REQUEST_TIMEOUT = 5  # seconds

# ============================================
# LOGGING
# ============================================
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(message)s",
    handlers=[logging.FileHandler(LOG_FILE), logging.StreamHandler()],
)

# ============================================
# DATABASE (WEB-BASED)
# ============================================
class WebhookDatabase:
    def __init__(self, webhook_url):
        self.webhook_url = webhook_url
        self.offline = False

        # Soft test on startup – never block server from starting
        try:
            resp = requests.post(
                webhook_url,
                json={"action": "test", "student_id": "ping"},
                timeout=REQUEST_TIMEOUT,
                allow_redirects=True,
            )
            logging.info(
                "Webhook test status: %s, body preview: %r",
                resp.status_code,
                resp.text[:120],
            )
        except Exception as e:
            logging.warning(f"Webhook test failed (starting anyway): {e}")
            # We *try* online mode; methods will flip offline if failures persist.

    def can_borrow(self, student_id):
        """Check if student can borrow (doesn't have a kit)"""
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

            # Even if status code is weird (302/405), try to parse JSON.
            try:
                result = resp.json()
            except ValueError:
                logging.error(
                    "Non-JSON response from webhook when checking borrow for %s: %r",
                    student_id,
                    resp.text[:200],
                )
                return False

            can_borrow = bool(result.get("can_borrow", False))
            logging.info("Check borrow %s: %s", student_id, can_borrow)
            return can_borrow

        except Exception as e:
            logging.error(f"Error checking borrow: {e}")
            self.offline = True
            return False

    def borrow_kit(self, student_id):
        """Mark student as having borrowed a kit"""
        if self.offline:
            logging.warning("Offline mode - not logging borrow to webhook")
            return False

        try:
            resp = requests.post(
                self.webhook_url,
                json={"action": "borrow", "student_id": student_id},
                timeout=REQUEST_TIMEOUT,
                allow_redirects=True,
            )
            logging.info(
                "Borrow POST for %s finished with status %s (body preview %r)",
                student_id,
                resp.status_code,
                resp.text[:120],
            )
            # Side-effect on Apps Script already happened even if status!=200.
            return True

        except Exception as e:
            logging.error(f"Error recording borrow: {e}")
            self.offline = True
            return False

    def return_kit(self, student_id):
        """Mark student as having returned a kit"""
        # Returns always allowed (even offline for fairness)
        try:
            if not self.offline:
                resp = requests.post(
                    self.webhook_url,
                    json={"action": "return", "student_id": student_id},
                    timeout=REQUEST_TIMEOUT,
                    allow_redirects=True,
                )
                logging.info(
                    "Return POST for %s finished with status %s (body preview %r)",
                    student_id,
                    resp.status_code,
                    resp.text[:120],
                )

            logging.info("%s returned kit", student_id)
            return True

        except Exception as e:
            logging.warning(f"Return logging failed, but allowing: {e}")
            # Still allow return even if webhook fails
            self.offline = True
            return True


# ============================================
# BLUETOOTH SERVER
# ============================================
class BluetoothServer:
    def __init__(self, database):
        self.db = database
        self.socket = None
        self.client = None

    def start(self):
        """Start Bluetooth server"""
        # Create RFCOMM socket
        self.socket = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        self.socket.bind(("", BLUETOOTH_PORT))
        self.socket.listen(1)

        # Register SPP service via sdptool instead of advertise_service()
        try:
            subprocess.run(
                ["sdptool", "add", "--channel", str(BLUETOOTH_PORT), "SP"],
                check=True,
            )
            logging.info(
                "Registered RFCOMM service with sdptool on channel %d",
                BLUETOOTH_PORT,
            )
        except Exception as e:
            logging.warning(
                "Failed to register service with sdptool (device may still connect if it knows MAC+channel): %s",
                e,
            )

        logging.info("Bluetooth server ready on port %d", BLUETOOTH_PORT)
        logging.info("Waiting for locker connection...")

        # Main loop
        while True:
            try:
                # Accept connection
                self.client, address = self.socket.accept()
                logging.info("Connected to %s", address)

                # Handle messages
                while True:
                    data = self.client.recv(1024)
                    if not data:
                        break

                    message = data.decode("utf-8").strip()
                    if message:  # Ignore empty messages
                        response = self.process(message)
                        self.client.send((response + "\n").encode("utf-8"))
                        logging.info("[%s] → %s", message, response)

            except Exception as e:
                logging.error(f"Connection error: {e}")
            finally:
                if self.client:
                    self.client.close()
                    self.client = None
                logging.info("Disconnected")

    def process(self, message):
        """Process message from locker"""
        # Message types:
        # "123456" = Student ID requesting borrow
        # "RETURN,123456" = Student returning kit
        # "BORROW,123456,timestamp" = Log entry (we can ignore)
        # "RETURN,123456,timestamp" = Log entry (we process as return)
        # "SECURITY_BREACH,UNKNOWN" = Security alert (log only)

        parts = message.split(",")

        if parts[0] == "RETURN":
            # Return request
            if len(parts) >= 2:
                student_id = parts[1]
                self.db.return_kit(student_id)
                return "OK"
            else:
                return "DENIED"

        elif parts[0] == "BORROW":
            # Borrow log entry – just acknowledge
            return "OK"

        elif parts[0] == "SECURITY_BREACH":
            # Security alert - log it
            logging.warning("SECURITY BREACH detected")
            return "OK"

        else:
            # Assume it's a student ID for borrow request
            student_id = parts[0]

            # Validate it's numeric and reasonable length
            if student_id.isdigit() and 4 <= len(student_id) <= 12:
                if self.db.can_borrow(student_id):
                    self.db.borrow_kit(student_id)
                    return "OK"
                else:
                    return "DENIED"
            else:
                logging.warning("Invalid student ID format: %s", message)
                return "DENIED"


# ============================================
# MAIN
# ============================================
if __name__ == "__main__":
    logging.info("=" * 50)
    logging.info("SMART LOCKER SERVER (GOOGLE SHEETS)")
    logging.info("=" * 50)

    # Initialize
    db = WebhookDatabase(WEBHOOK_URL)
    server = BluetoothServer(db)

    # Run
    try:
        server.start()
    except KeyboardInterrupt:
        logging.info("\nServer stopped")

