#!/usr/bin/env python3
"""
Minimal Smart Locker Server for Raspberry Pi
Just tracks student ID and whether they have a kit borrowed
"""

import json
import time
import logging
import bluetooth
import firebase_admin
from firebase_admin import credentials, firestore

# ============================================
# CONFIG
# ============================================
BLUETOOTH_PORT = 1
FIREBASE_CREDS = "serviceAccountKey.json"  # Download from Firebase
LOG_FILE = "locker.log"

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
# DATABASE (SUPER SIMPLE)
# ============================================
class Database:
    def __init__(self):
        self.db = None
        self.offline = False
        
        try:
            # Initialize Firebase
            cred = credentials.Certificate(FIREBASE_CREDS)
            firebase_admin.initialize_app(cred)
            self.db = firestore.client()
            logging.info("✅ Connected to Firebase")
        except Exception as e:
            logging.error(f"❌ Firebase failed, running offline: {e}")
            self.offline = True
    
    def can_borrow(self, student_id):
        """Check if student can borrow (doesn't have a kit)"""
        if self.offline:
            # Offline = deny borrows for fairness
            return False
        
        try:
            # Get student document
            doc = self.db.collection('students').document(student_id).get()
            
            if not doc.exists:
                # New student - create entry
                self.db.collection('students').document(student_id).set({
                    'has_kit': False
                })
                return True
            
            # Check if they already have a kit
            data = doc.to_dict()
            return not data.get('has_kit', False)
            
        except:
            self.offline = True
            return False
    
    def borrow_kit(self, student_id):
        """Mark student as having borrowed a kit"""
        if self.offline:
            return False
        
        try:
            self.db.collection('students').document(student_id).set({
                'has_kit': True
            })
            logging.info(f"✅ {student_id} borrowed kit")
            return True
        except:
            return False
    
    def return_kit(self, student_id):
        """Mark student as having returned a kit"""
        # Returns always allowed (even offline)
        try:
            if not self.offline:
                self.db.collection('students').document(student_id).set({
                    'has_kit': False
                })
            logging.info(f"✅ {student_id} returned kit")
            return True
        except:
            # Still allow return even if DB fails
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
        
        logging.info(f"📡 Bluetooth server ready on port {BLUETOOTH_PORT}")
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
                    
                    message = data.decode('utf-8').strip()
                    response = self.process(message)
                    
                    self.client.send((response + '\n').encode('utf-8'))
                    logging.info(f"[{message}] → {response}")
                
            except Exception as e:
                logging.error(f"Error: {e}")
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
        
        if message.startswith("RETURN,"):
            # Return request
            student_id = message.split(',')[1]
            self.db.return_kit(student_id)
            return "OK"
        else:
            # Borrow request (just student ID)
            student_id = message
            if self.db.can_borrow(student_id):
                self.db.borrow_kit(student_id)
                return "OK"
            else:
                return "DENIED"

# ============================================
# MAIN
# ============================================
if __name__ == "__main__":
    logging.info("="*40)
    logging.info("SMART LOCKER SERVER (MINIMAL)")
    logging.info("="*40)
    
    # Initialize
    db = Database()
    server = BluetoothServer(db)
    
    # Run
    try:
        server.start()
    except KeyboardInterrupt:
        logging.info("\n👋 Server stopped")
