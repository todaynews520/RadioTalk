# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-based dual-mode radio device that combines:
- **FM Radio receiver** using RDA5807 chip (87-108 MHz range)
- **Two-way radio transceiver** using A1846S chip (VHF/UHF/WHF bands)

The device features a TFT LCD display, rotary encoder for frequency/volume control, and multiple buttons for mode switching and channel selection. It supports both receive and transmit modes for the two-way radio functionality.

## Development Commands

### Building and Uploading
```bash
# Build the project
platformio run

# Upload to ESP32 device
platformio run --target upload

# Build and upload in one command
platformio run --target upload --environment esp32dev
```

### Testing
This project currently has no unit tests implemented. The test directory contains only a placeholder README. Manual testing is required by uploading to hardware and verifying functionality.

### Serial Monitoring
```bash
# Monitor serial output from ESP32
platformio device monitor

# Monitor with specific baud rate (115200 as used in code)
platformio device monitor -b 115200
```

### Library Management
```bash
# Install/update libraries specified in platformio.ini
platformio lib install

# Update all libraries
platformio lib update
```

## Code Architecture

### Main Components
1. **Hardware Abstraction**:
   - `RDA5807.h` - FM radio receiver library
   - `HamShield.h` - A1846S two-way radio transceiver library
   - `TFT_eSPI.h` - Display driver for ST7789/GC9307 LCD
   - `OneButton.h` - Button debouncing and multi-click detection
   - `RotaryEncoder.h` - Rotary encoder input handling
   - `Battery18650Stats.h` - Battery level monitoring

2. **Global State Management**:
   - `state_1846_5087` - Boolean flag indicating current mode (true=RDA5807 FM radio, false=A1846S two-way radio)
   - `modetx` - Boolean flag for A1846S transmit vs receive mode
   - `adj_volume` - Boolean flag for volume adjustment mode
   - `seeking` - Boolean flag for auto-seek mode
   - Various frequency arrays (`station_1846_tx`, `station_1846_rx`, `station_5807`) for preset channels

3. **Persistent Storage**:
   - Uses `Preferences` library to store settings across reboots
   - Stores: last frequencies, volume levels, calibration factors, preset channels

4. **User Interface Flow**:
   - **Setup**: Initializes hardware, loads preferences, sets up button callbacks
   - **Main Loop**: Handles button events, encoder input, and timer-based updates
   - **Display**: Single `drawSprite()` function handles all screen rendering based on current state

### Key Functionality Patterns

#### Mode Switching
- Long press UP button toggles between RDA5807 (FM radio) and A1846S (two-way radio) modes
- Double click DOWN button toggles A1846S between transmit and receive modes

#### Frequency Control
- Rotary encoder adjusts frequency when not in volume/seek modes
- Different step sizes configurable via single click on encoder
- Preset channels accessible via UP/DOWN buttons

#### Volume Control
- Double click UP button enters volume adjustment mode
- Rotary encoder adjusts volume in this mode
- Automatic power management for audio amplifier

#### Power Management
- Long press encoder button powers down entire system
- LCD backlight auto-off after 20 seconds of inactivity
- Individual power control for RDA5807 and A1846S chips

### Hardware Pin Mapping
- **Buttons**: SW1 (32), SW2 (35), SW3 (16), PUSH (36)
- **Rotary Encoder**: PIN1 (34), PIN2 (39)
- **I2C**: SDA (21), SCL (22)
- **Power Control**: EN_5807 (27), EN_1846 (26), PWR_ON (12)
- **Audio**: AMP_PIN (33)
- **Display**: LCD_BLK (4), LCD_CS (15)

## Important Notes

- The project uses non-standard I2C initialization due to HamShield library requirements
- Frequency calibration is implemented for the A1846S chip to correct for oscillator drift
- Regulatory compliance note in code: "本项目仅供学习，请勿在任何可能影响无线电正常通信的地方测试此项目"
- Battery voltage measurement uses ADC pin 25 with configurable conversion factor
- Signal strength indicators are mapped differently for RDA5807 (0-99 RSSI) vs A1846S (dBm values)