#!/usr/bin/env python3
"""
SmartLocker Pi Server
=====================
Bridges the TTGO Bluetooth connection to the Google Sheets webhook.

This script:
1. Reads from /dev/rfcomm0 (TTGO Bluetooth connection)
2. Parses commands (BORROW,{id} or RETURN,{id})
3. Makes HTTP requests to Google Apps Script webhook
4. Sends responses back to TTGO (OK or DENIED)

BORROW logic:
- Student must EXIST in the database
- Student must NOT already have a box borrowed
- If both conditions met -> OK, else -> DENIED

RETURN logic:
- Just record the return -> OK
"""

import os
import sys
import time
import json
import signal
import logging
from datetime import datetime
from pathlib import Path

try:
    import requests
except ImportError:
    print("ERROR: 'requests' library not found")
    print("Install with: pip3 install requests")
    sys.exit(1)

# Configuration
RFCOMM_DEVICE = "/dev/rfcomm0"
LOCAL_LOG_FILE = Path.home() / "smartlocker_log.json"
WEBHOOK_TIMEOUT = 10
MAX_RETRIES = 3
RETRY_DELAY = 1
DEFAULT_WEBHOOK_URL = "https://script.google.com/macros/s/AKfycbwU7jvZcrGItGfxu3uS4Ux9vrXrL5ne9Lh0TXLLuW8OUCVsh6H6-UAUgRck5Nj89nfssw/exec"

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(Path.home() / "smartlocker_server.log")
    ]
)
logger = logging.getLogger(__name__)


class LocalLog:
    """Local log for offline operation and audit trail"""
    
    def __init__(self, filepath):
        self.filepath = Path(filepath)
        self.pending_syncs = []
        self._load()
    
    def _load(self):
        if self.filepath.exists():
            try:
                with open(self.filepath, 'r') as f:
                    data = json.load(f)
                    self.pending_syncs = data.get('pending', [])
                    logger.info(f"Loaded {len(self.pending_syncs)} pending syncs from local log")
            except Exception as e:
                logger.warning(f"Could not load local log: {e}")
                self.pending_syncs = []
    
    def _save(self):
        try:
            with open(self.filepath, 'w') as f:
                json.dump({
                    'pending': self.pending_syncs,
                    'last_updated': datetime.now().isoformat()
                }, f, indent=2)
        except Exception as e:
            logger.error(f"Could not save local log: {e}")
    
    def add_pending(self, action, student_id):
        self.pending_syncs.append({
            'action': action,
            'student_id': student_id,
            'timestamp': datetime.now().isoformat()
        })
        self._save()
        logger.info(f"Added pending: {action} for {student_id}")
    
    def remove_pending(self, action, student_id):
        self.pending_syncs = [
            p for p in self.pending_syncs
            if not (p['action'] == action and p['student_id'] == student_id)
        ]
        self._save()
    
    def get_pending(self):
        return self.pending_syncs.copy()


class WebhookClient:
    """Client for Google Apps Script webhook"""
    
    def __init__(self, webhook_url):
        self.webhook_url = webhook_url
        self.session = requests.Session()
    
    def _post(self, data):
        """Make POST request with retries"""
        for attempt in range(MAX_RETRIES):
            try:
                response = self.session.post(
                    self.webhook_url,
                    json=data,
                    timeout=WEBHOOK_TIMEOUT
                )
                
                if response.status_code == 200:
                    return response.json()
                else:
                    logger.warning(f"Webhook returned {response.status_code}: {response.text}")
                    
            except requests.exceptions.Timeout:
                logger.warning(f"Webhook timeout (attempt {attempt + 1}/{MAX_RETRIES})")
            except requests.exceptions.RequestException as e:
                logger.warning(f"Webhook error (attempt {attempt + 1}/{MAX_RETRIES}): {e}")
            
            if attempt < MAX_RETRIES - 1:
                time.sleep(RETRY_DELAY)
        
        return None
    
    def check_borrow(self, student_id):
        """
        Check if student can borrow.
        Returns tuple: (can_borrow: bool, reason: str)
        
        Rules:
        - Student must EXIST in database
        - Student must NOT already have a box
        """
        logger.info(f"Checking borrow eligibility for {student_id}")
        
        result = self._post({
            'action': 'check_borrow',
            'student_id': student_id
        })
        
        if result:
            can_borrow = result.get('can_borrow', False)
            message = result.get('message', '')
            
            # Check if this is a new student (not in database)
            if 'new student' in message.lower():
                logger.info(f"Student {student_id} not found in database - DENIED")
                return False, "not_registered"
            
            logger.info(f"Check result: can_borrow={can_borrow}, message={message}")
            
            if can_borrow:
                return True, "ok"
            else:
                return False, "already_borrowed"
        
        # Webhook unreachable - DENY for safety (can't verify student)
        logger.warning("Could not verify student - DENYING for safety")
        return False, "offline"
    
    def record_borrow(self, student_id):
        """Record a borrow transaction"""
        logger.info(f"Recording borrow for {student_id}")
        
        result = self._post({
            'action': 'borrow',
            'student_id': student_id
        })
        
        if result:
            logger.info(f"Borrow recorded: {result}")
            return True
        
        return False
    
    def record_return(self, student_id):
        """Record a return transaction"""
        logger.info(f"Recording return for {student_id}")
        
        result = self._post({
            'action': 'return',
            'student_id': student_id
        })
        
        if result:
            logger.info(f"Return recorded: {result}")
            return True
        
        return False


class LockerServer:
    """Main server that bridges TTGO Bluetooth to webhook"""
    
    def __init__(self, webhook_url):
        self.webhook = WebhookClient(webhook_url)
        self.local_log = LocalLog(LOCAL_LOG_FILE)
        self.rfcomm = None
        self.running = False
    
    def open_rfcomm(self):
        try:
            if not os.path.exists(RFCOMM_DEVICE):
                logger.info(f"Waiting for {RFCOMM_DEVICE}...")
                return False
            
            self.rfcomm = open(RFCOMM_DEVICE, 'r+b', buffering=0)
            logger.info(f"Opened {RFCOMM_DEVICE}")
            return True
            
        except Exception as e:
            logger.error(f"Could not open {RFCOMM_DEVICE}: {e}")
            return False
    
    def close_rfcomm(self):
        if self.rfcomm:
            try:
                self.rfcomm.close()
            except:
                pass
            self.rfcomm = None
    
    def send_response(self, response):
        if self.rfcomm:
            try:
                msg = f"{response}\n".encode('utf-8')
                self.rfcomm.write(msg)
                self.rfcomm.flush()
                logger.info(f"Sent to TTGO: {response}")
                return True
            except Exception as e:
                logger.error(f"Failed to send response: {e}")
        return False
    
    def handle_borrow(self, student_id):
        """Handle a borrow request"""
        logger.info(f"=== BORROW REQUEST: {student_id} ===")
        
        # Check if student can borrow
        can_borrow, reason = self.webhook.check_borrow(student_id)
        
        if can_borrow:
            # Record the borrow
            if self.webhook.record_borrow(student_id):
                self.send_response("OK")
                logger.info(f"BORROW APPROVED for {student_id}")
            else:
                # Webhook down for recording - still approve but log locally
                self.local_log.add_pending('borrow', student_id)
                self.send_response("OK")
                logger.warning(f"BORROW APPROVED (logged locally) for {student_id}")
        else:
            self.send_response("DENIED")
            if reason == "not_registered":
                logger.info(f"BORROW DENIED for {student_id} - Student not in database")
            elif reason == "already_borrowed":
                logger.info(f"BORROW DENIED for {student_id} - Already has a box")
            else:
                logger.info(f"BORROW DENIED for {student_id} - {reason}")
    
    def handle_return(self, student_id):
        """Handle a return notification"""
        logger.info(f"=== RETURN NOTIFICATION: {student_id} ===")
        
        if self.webhook.record_return(student_id):
            self.send_response("OK")
            logger.info(f"RETURN RECORDED for {student_id}")
        else:
            # Webhook down - log locally for later sync
            self.local_log.add_pending('return', student_id)
            self.send_response("OK")
            logger.warning(f"RETURN LOGGED LOCALLY for {student_id}")
    
    def sync_pending(self):
        """Try to sync any pending local transactions"""
        pending = self.local_log.get_pending()
        if not pending:
            return
        
        logger.info(f"Syncing {len(pending)} pending transactions...")
        
        for item in pending:
            action = item['action']
            student_id = item['student_id']
            
            success = False
            if action == 'borrow':
                success = self.webhook.record_borrow(student_id)
            elif action == 'return':
                success = self.webhook.record_return(student_id)
            
            if success:
                self.local_log.remove_pending(action, student_id)
                logger.info(f"Synced: {action} for {student_id}")
            else:
                logger.warning(f"Could not sync: {action} for {student_id}")
    
    def process_message(self, message):
        """Process a message from TTGO"""
        message = message.strip().upper()
        
        if not message:
            return
        
        logger.info(f"Received from TTGO: {message}")
        
        if ',' in message:
            parts = message.split(',', 1)
            command = parts[0]
            student_id = parts[1] if len(parts) > 1 else ""
        else:
            # Legacy: just a student ID (assume borrow)
            command = "BORROW"
            student_id = message
        
        if not student_id or len(student_id) < 8:
            logger.warning(f"Invalid student ID: {student_id}")
            self.send_response("DENIED")
            return
        
        if command == "BORROW":
            self.handle_borrow(student_id)
        elif command == "RETURN":
            self.handle_return(student_id)
        else:
            logger.warning(f"Unknown command: {command}")
    
    def run(self):
        """Main server loop"""
        self.running = True
        buffer = b""
        last_sync_attempt = 0
        
        logger.info("=" * 50)
        logger.info("SmartLocker Pi Server Starting")
        logger.info(f"Webhook: {self.webhook.webhook_url}")
        logger.info(f"Device: {RFCOMM_DEVICE}")
        logger.info("=" * 50)
        
        while self.running:
            try:
                if not self.rfcomm:
                    if not self.open_rfcomm():
                        time.sleep(1)
                        continue
                
                now = time.time()
                if now - last_sync_attempt > 60:
                    self.sync_pending()
                    last_sync_attempt = now
                
                try:
                    data = self.rfcomm.read(1)
                    if data:
                        if data == b'\n':
                            try:
                                message = buffer.decode('utf-8')
                                self.process_message(message)
                            except:
                                pass
                            buffer = b""
                        elif data != b'\r':
                            buffer += data
                    else:
                        logger.warning("TTGO disconnected")
                        self.close_rfcomm()
                        time.sleep(1)
                        
                except Exception as e:
                    logger.error(f"Read error: {e}")
                    self.close_rfcomm()
                    time.sleep(1)
                    
            except KeyboardInterrupt:
                logger.info("Shutting down...")
                self.running = False
                
            except Exception as e:
                logger.error(f"Server error: {e}")
                time.sleep(1)
        
        self.close_rfcomm()
        logger.info("Server stopped")
    
    def stop(self):
        self.running = False


def main():
    if len(sys.argv) > 1:
        webhook_url = sys.argv[1]
    else:
        webhook_url = DEFAULT_WEBHOOK_URL
        print("Using default webhook URL")
    
    server = LockerServer(webhook_url)
    
    def signal_handler(sig, frame):
        logger.info("Received shutdown signal")
        server.stop()
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    server.run()


if __name__ == "__main__":
    main()
