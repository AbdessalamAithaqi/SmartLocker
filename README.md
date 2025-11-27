# SmartLocker System

A smart locker system using ESP32 TTGO and Raspberry Pi 5 with Google Sheets integration.

## Architecture

```
┌─────────────────┐     Bluetooth SPP     ┌─────────────────┐     HTTPS      ┌─────────────────┐
│   ESP32 TTGO    │ ◄──────────────────► │  Raspberry Pi   │ ◄────────────► │  Google Sheets  │
│    (Server)     │                       │    (Client)     │                │   + Apps Script │
└─────────────────┘                       └─────────────────┘                └─────────────────┘
       │                                         │
       │                                         │
   ┌───┴───┐                               ┌─────┴─────┐
   │Hardware│                               │  Webhook  │
   │ - LCD  │                               │  Handler  │
   │ - Keypad│                              └───────────┘
   │ - Servo │
   │ - LEDs  │
   │ - IR    │
   └─────────┘
```

## Components

### Hardware (TTGO)
- ESP32 TTGO T-Display
- 16x2 LCD with I2C (0x27)
- 4x4 Matrix Keypad
- Green LED (status indicator)
- Servo motor (door lock)
- 2x IR sensors (box detection, door detection)

### Software
- **TTGO**: Arduino sketch running as Bluetooth SPP Server
- **Pi**: Python server bridging Bluetooth to Google Sheets webhook
- **Cloud**: Google Apps Script webhook with Google Sheets database

## Communication Protocol

### TTGO → Pi
```
BORROW,{student_id}\n    - Request to borrow a kit
RETURN,{student_id}\n    - Notify that kit was returned
```

### Pi → TTGO
```
OK\n                     - Authorization granted / action successful
DENIED\n                 - Authorization denied
```

---

## Setup Instructions

### 1. Google Sheets Setup

1. Create a new Google Sheet
2. Go to **Extensions → Apps Script**
3. Paste the code from `apps_script.js` (or use the webhook code you have)
4. Click **Deploy → New deployment**
5. Choose **Web app**
6. Set **Execute as**: Me
7. Set **Who has access**: Anyone
8. Click **Deploy** and copy the URL

Test the webhook:
```bash
python3 test_webhook.py "YOUR_WEBHOOK_URL"
```

### 2. TTGO Setup

#### Required Arduino Libraries
Install via Arduino IDE Library Manager:
- `BluetoothSerial` (built-in)
- `ESP32Servo`
- `Keypad`
- `LiquidCrystal_I2C`
- `Wire` (built-in)

#### Upload the Code
1. Open `SmartLocker_TTGO.ino` in Arduino IDE
2. Select board: **ESP32 Dev Module** (or your TTGO variant)
3. Upload all files:
   - `SmartLocker_TTGO.ino`
   - `config.h`
   - `bt_comm.h` / `bt_comm.cpp`
   - `lcd_display.h` / `lcd_display.cpp`
   - `keypad.h` / `keypad.cpp`
   - `led.h` / `led.cpp`
   - `servo.h` / `servo.cpp`
   - `ir.h` / `ir.cpp`

4. Open Serial Monitor (115200 baud)
5. **Note the MAC address** shown on boot - you'll need this for the Pi

Example output:
```
=================================
Smart Locker TTGO Starting...
Mode: Bluetooth SERVER
=================================
[BT] Server started as 'SmartLockerTTGO'
[BT] Waiting for Pi to connect...
[BT] MAC Address: 14:2B:2F:A7:94:22    ← COPY THIS
```

### 3. Raspberry Pi Setup

#### Install Dependencies
```bash
sudo apt update
sudo apt install -y bluetooth bluez python3-pip
pip3 install requests
```

#### Copy Files
```bash
sudo mkdir -p /opt/smartlocker
sudo cp locker_server.py start_locker.sh /opt/smartlocker/
sudo chmod +x /opt/smartlocker/start_locker.sh
```

#### Test Connection
```bash
# Replace with YOUR TTGO's MAC address
sudo /opt/smartlocker/start_locker.sh "14:2B:2F:A7:94:22" "YOUR_WEBHOOK_URL"
```

#### Setup Auto-Start
```bash
# Edit the service file with your values
sudo nano /etc/systemd/system/smartlocker.service

# Paste contents of smartlocker.service and update:
# - TTGO_MAC=your_mac_address
# - WEBHOOK_URL=your_webhook_url

# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable smartlocker
sudo systemctl start smartlocker

# Check status
sudo systemctl status smartlocker
sudo journalctl -u smartlocker -f
```

---

## Pin Configuration

| Component      | Pin(s)          | Notes                    |
|----------------|-----------------|--------------------------|
| LCD SDA        | GPIO 21         | I2C Data                 |
| LCD SCL        | GPIO 22         | I2C Clock                |
| Green LED      | GPIO 23         | Status indicator         |
| Keypad Rows    | 14, 12, 13, 15  | R0-R3                    |
| Keypad Cols    | 2, 0, 25, 4     | C0-C3                    |
| Box IR Sensor  | GPIO 34         | Analog, threshold: 2800  |
| Door IR Sensor | GPIO 35         | Analog, threshold: 1300  |
| Servo          | GPIO 26         | Lock: 0°, Unlock: 90°    |

---

## User Flow

### Borrowing a Kit
1. User sees "Box Available - Press # to start"
2. User presses `#`
3. User enters 8-9 digit student ID
4. User presses `#` to confirm (or `*` to backspace, `D` to cancel)
5. System sends ID to Pi → Pi checks with Google Sheets
6. If approved: Door unlocks, user takes box, closes door
7. If denied: "Access Denied - Already borrowed"

### Returning a Kit
1. User sees "Box Out - # to return"
2. User presses `#`
3. Door unlocks
4. User places box inside, closes door
5. System logs return to Google Sheets

---

## Troubleshooting

### TTGO won't connect to Pi
- Ensure TTGO is powered and showing "Waiting for Pi connection"
- Verify MAC address is correct in Pi configuration
- On Pi, check: `sudo hciconfig hci0 up`
- Try: `bluetoothctl scan on` to see if TTGO appears

### Pi can't reach webhook
- Test webhook directly: `curl -X POST -H "Content-Type: application/json" -d '{"action":"test"}' YOUR_URL`
- Check internet connection
- Verify Apps Script deployment is set to "Anyone"

### Door won't unlock
- Check servo power supply (may need external 5V)
- Verify servo angle settings in `config.h`
- Test servo manually via Serial commands

### IR sensors not detecting
- Use Serial Monitor to see raw sensor values
- Adjust `IR_BOX_THRESHOLD` and `IR_DOOR_THRESHOLD` in `config.h`
- Ensure sensors are properly positioned

### Keypad not responding
- Check wiring matches `config.h` pin definitions
- Test individual keys via Serial output

---

## Files Overview

| File                   | Purpose                                      |
|------------------------|----------------------------------------------|
| `SmartLocker_TTGO.ino` | Main Arduino sketch (state machine)          |
| `config.h`             | Pin definitions and timing constants         |
| `bt_comm.h/cpp`        | Bluetooth SPP Server communication           |
| `lcd_display.h/cpp`    | LCD display wrapper                          |
| `keypad.h/cpp`         | Keypad input wrapper                         |
| `led.h/cpp`            | LED control                                  |
| `servo.h/cpp`          | Door servo control                           |
| `ir.h/cpp`             | IR sensor wrappers                           |
| `locker_server.py`     | Pi server (BT to webhook bridge)             |
| `start_locker.sh`      | Pi startup script                            |
| `smartlocker.service`  | Systemd service for auto-start               |
| `test_webhook.py`      | Webhook testing tool                         |

---

## License

MIT License - Feel free to modify and use for your projects!
