# Smart Door Lock System

An Arduino-based smart door lock with PIN access control, LCD display, and brute-force protection. Designed to run on the [Wokwi simulator](https://wokwi.com) or on real hardware.

## Features

- **PIN-based access control** -- enter a 4-digit code on a membrane keypad to unlock
- **Servo lock mechanism** -- simulated bolt that rotates gradually for realism
- **LCD feedback** -- 16x2 I2C LCD shows PIN entry, status messages, and countdowns
- **Auto-relock** -- door re-locks automatically after 5 seconds
- **Brute-force protection** -- 3 wrong attempts triggers a 30-second lockout
- **PIN change workflow** -- press `#` to change the PIN (old PIN verification + new PIN confirmation)
- **Visual & audio feedback** -- green/red LEDs and buzzer tones for all events
- **EEPROM persistence** -- master PIN survives power cycles

## Hardware

| Component | Details |
|---|---|
| Arduino Uno | Target microcontroller |
| 4x3 Membrane Keypad | PIN input |
| Servo Motor (SG90) | Lock bolt mechanism |
| LCD 1602 + I2C Adapter | Status display (address `0x27`) |
| Green LED + 220Ω resistor | Unlocked indicator |
| Red LED + 220Ω resistor | Error/lockout indicator |
| Piezo Buzzer | Audio feedback |

### Pin Mapping

| Pin | Component |
|---|---|
| D3, D4, D5 | Keypad columns C1-C3 |
| D6, D7, D8, D9 | Keypad rows R1-R4 |
| D10 | Servo (PWM) |
| D11 | Green LED |
| D12 | Red LED |
| D13 | Buzzer |
| A4 (SDA) | LCD I2C data |
| A5 (SCL) | LCD I2C clock |

## Getting Started

### Run on Wokwi (recommended)

1. Open the project on [Wokwi](https://wokwi.com/projects/472213884349910017)
2. Click **Start** to simulate

### Run on real hardware

1. Install [Arduino CLI](https://arduino.github.io/arduino-cli/) or the Arduino IDE
2. Install required libraries:
   ```
   arduino-cli lib install Servo
   arduino-cli lib install Keypad
   arduino-cli lib install LiquidCrystal I2C
   ```
3. Compile and upload:
   ```
   arduino-cli compile --fqbn arduino:avr:uno sketch.ino
   arduino-cli upload --fqbn arduino:avr:uno sketch.ino
   ```
4. Wire components according to the pin mapping above

## Usage

1. On startup, the LCD displays `ENTER PIN:`
2. Type a 4-digit PIN on the keypad; digits appear as `*`
3. Press `*` to clear input, or `#` to enter PIN change mode
4. On correct PIN: door unlocks, green LED turns on, LCD shows countdown
5. On wrong PIN: red LED flashes, LCD shows remaining attempts
6. After 3 wrong attempts: system locks out for 30 seconds

**Default PIN:** `1234`

## Configuration

All settings are defined as constants in `sketch.ino`:

| Constant | Default | Description |
|---|---|---|
| `PIN_LEN` | 4 | PIN code length |
| `MAX_ATTEMPTS` | 3 | Wrong attempts before lockout |
| `LOCKOUT_MS` | 30000 | Lockout duration (ms) |
| `AUTO_RELOCK_MS` | 5000 | Auto re-lock delay (ms) |
| `SERVO_STEP_MS` | 15 | Servo step interval (ms/degree) |

## Project Structure

```
├── sketch.ino          # Main Arduino firmware
├── diagram.json        # Wokwi circuit diagram & wiring
├── libraries.txt       # Wokwi library dependencies
├── wokwi.toml          # Wokwi simulation config
└── build/              # Compiled binaries
    ├── sketch.ino.hex
    └── sketch.ino.elf
```

## License

MIT
