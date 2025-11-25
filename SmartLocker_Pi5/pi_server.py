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

# ============================================
# CONFIG
# ============================================
BLUETOOTH_PORT = 1
LOG_FILE = "locker.log"

# !!! IMPORTANT: Replace this with your Apps Script Web App URL !!!
# Deploy your Apps Script as a web app and paste the URL here
WEBHOOK_URL = "https://script.google.com/macros/s/YOUR_DEPLOYMENT_ID/exec"

# Timeout for webhook requests
REQUEST_TIMEOUT = 5  # seconds

# ============================================
# LOGGING
# ============================================
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE),
        logging.StreamHandler()
    ]
)

# ============================================
# DATABASE (WEB-BASED)
# ============================================
class WebhookDatabase:
    def __init__(self, webhook_url):
        self.webhook_url = webhook_url
        self.offline = False
        
        # Test connection on startup
        try:
            response = requests.post(
                webhook_url,
                json={"action": "test"},
                timeout=REQUEST_TIMEOUT
            )
            if response.status_code == 200:
                logging.info("Connected to Google Sheets webhook")
                self.offline = False
            else:
                logging.warning(f"Webhook returned {response.status_code}")
                self.offline = True
        except Exception as e:
            logging.error(f"Webhook connection failed: {e}")
            self.offline = True
    
    def can_borrow(self, student_id):
        """Check if student can borrow (doesn't have a kit)"""
        if self.offline:
            logging.warning("Offline mode - denying borrow")
            return False
        
        try:
            # Send request to Apps Script
            response = requests.post(
                self.webhook_url,
                json={
                    "action": "check_borrow",
                    "student_id": student_id
                },
                timeout=REQUEST_TIMEOUT
            )
            
            if response.status_code != 200:
                logging.error(f"Webhook error: {response.status_code}")
                self.offline = True
                return False
            
            result = response.json()
            can_borrow = result.get("can_borrow", False)
            
            logging.info(f"Check borrow {student_id}: {can_borrow}")
            return can_borrow
            
        except Exception as e:
            logging.error(f"Error checking borrow: {e}")
            self.offline = True
            return False
    
    def borrow_kit(self, student_id):
        """Mark student as having borrowed a kit"""
        if self.offline:
            return False
        
        try:
            response = requests.post(
                self.webhook_url,
                json={
                    "action": "borrow",
                    "student_id": student_id
                },
                timeout=REQUEST_TIMEOUT
            )
            
            if response.status_code == 200:
                logging.info(f"{student_id} borrowed kit")
                return True
            else:
                logging.error(f"Borrow failed: {response.status_code}")
                return False
                
        except Exception as e:
            logging.error(f"Error recording borrow: {e}")
            return False
    
    def return_kit(self, student_id):
        """Mark student as having returned a kit"""
        # Returns always allowed (even offline for fairness)
        try:
            if not self.offline:
                response = requests.post(
                    self.webhook_url,
                    json={
                        "action": "return",
                        "student_id": student_id
                    },
                    timeout=REQUEST_TIMEOUT
                )
                
                if response.status_code != 200:
                    logging.warning(f"Return webhook failed, but allowing anyway")
            
            logging.info(f"{student_id} returned kit")
            return True
            
        except Exception as e:
            logging.warning(f"Return logging failed, but allowing: {e}")
            # Still allow return even if webhook fails
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
        # Create socket
        self.socket = bluetooth.BluetoothSocket(bluetooth.RFCOMM)
        self.socket.bind(("", BLUETOOTH_PORT))
        self.socket.listen(1)
        
        # Advertise service
        bluetooth.advertise_service(
            self.socket,
            "SmartLocker",
            service_id="00001101-0000-1000-8000-00805F9B34FB",
            service_classes=["00001101-0000-1000-8000-00805F9B34FB", bluetooth.SERIAL_PORT_CLASS],
            profiles=[bluetooth.SERIAL_PORT_PROFILE]
        )
        
        logging.info(f"Bluetooth server ready on port {BLUETOOTH_PORT}")
        logging.info("Waiting for locker connection...")
        
        # Main loop
        while True:
            try:
                # Accept connection
                self.client, address = self.socket.accept()
                logging.info(f"Connected to {address}")
                
                # Handle messages
                while True:
                    data = self.client.recv(1024)
                    if not data:
                        break
                    
                    message = data.decode('utf-8').strip()
                    if message:  # Ignore empty messages
                        response = self.process(message)
                        self.client.send((response + '\n').encode('utf-8'))
                        logging.info(f"[{message}] → {response}")
                
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
        
        parts = message.split(',')
        
        if parts[0] == "RETURN":
            # Return request
            if len(parts) >= 2:
                student_id = parts[1]
                self.db.return_kit(student_id)
                return "OK"
            else:
                return "DENIED"
        
        elif parts[0] == "BORROW":
            # Borrow log entry just acknowledge
            return "OK"
        
        elif parts[0] == "SECURITY_BREACH":
            # Security alert - log it
            logging.warning(f"SECURITY BREACH detected")
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
                logging.warning(f"Invalid student ID format: {message}")
                return "DENIED"

# ============================================
# MAIN
# ============================================
if __name__ == "__main__":
    logging.info("="*50)
    logging.info("SMART LOCKER SERVER (GOOGLE SHEETS)")
    logging.info("="*50)
    
    # Check webhook URL
    if "AKfycbzqHnNa_fO4LAayWhiWH0lL8KIDsCxuOL25IaVZCJ4ihY6Lo_CVwCIv0tb8SR197mJhmw" in WEBHOOK_URL:
        logging.error("ERROR: You must set your WEBHOOK_URL!")
        logging.error("   1. Deploy your Apps Script as a web app")
        logging.error("   2. Copy the deployment URL")
        logging.error("   3. Replace WEBHOOK_URL in this script")
        exit(1)
    
    # Initialize
    db = WebhookDatabase(WEBHOOK_URL)
    server = BluetoothServer(db)
    
    # Run
    try:
        server.start()
    except KeyboardInterrupt:
        logging.info("\nServer stopped")
