# RadioTalk - ESP32 Dual-Mode Radio Device

<div align="center">

  ![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32%20Dev-blue)
  ![License](https://img.shields.io/badge/license-MIT-green)
  ![Status](https://img.shields.io/badge/status-stable-success)

  A dual-mode radio device combining FM radio reception and two-way radio transceiver capabilities.

  [Features](#features) • [Hardware](#hardware) • [Building](#building) • [Usage](#usage) • [Safety](#safety-warning)

</div>

---

## Overview

This project is an ESP32-based portable radio device that supports:

- **FM Radio Reception** (87-108 MHz) via RDA5807 chip
- **Two-way Radio Transceiver** (VHF/UHF/WHF bands) via A1846S chip
- **TFT LCD Display** with real-time frequency and signal strength
- **Rotary Encoder** for precise frequency/volume control
- **Battery Monitoring** with automatic calibration

---

## Features

### Radio Modes

| Mode | Chip | Frequency Range | Channels |
|------|------|-----------------|----------|
| FM Radio | RDA5807 | 87-108 MHz | 20 presets |
| Two-way (TX) | A1846S | 134-520 MHz | 20 presets |
| Two-way (RX) | A1846S | 134-520 MHz | 20 presets |

### User Interface

- **TFT LCD Display** (ST7789/GC9307)
  - Frequency display with 6-channel view
  - Signal strength meter (S-meter)
  - Battery level indicator
  - Volume indicator
  - Stereo/RX/TX mode indicators

- **Rotary Encoder** (34/39)
  - Frequency adjustment (configurable step: 10/25/50/100/200/1000 kHz)
  - Volume adjustment (0-15)

- **Buttons**
  - **UP Button** - Mode switch, channel selection, volume mode
  - **MIDDLE Button** - Channel navigation, frequency save/delete, band switch
  - **DOWN Button** - TX enable, mode toggle, channel navigation
  - **ENCODER Button** - Step size cycling, seek mode, power off

### Safety Features

- Watchdog timer (30s timeout) prevents system hangs
- RF power limiting (safe TX power level)
- Frequency range validation before transmission
- I2C error handling with automatic retry
- Battery over-discharge protection

---

## Hardware

### Components

| Component | Description |
|-----------|-------------|
| MCU | ESP32-WROOM |
| FM Radio | RDA5807 |
| Two-way Radio | A1846S |
| Display | ST7789/GC9307 TFT LCD (320x170) |
| Rotary Encoder | EC11 |
| Battery | 18650 Li-ion |

### Pin Mapping

| Function | Pin | Notes |
|----------|-----|-------|
| SW1 (Middle) | GPIO 32 | Button |
| SW2 (Down) | GPIO 35 | Button |
| SW3 (Up) | GPIO 16 | Button |
| Push | GPIO 36 | Encoder button |
| RE_PIN1 | GPIO 34 | Encoder A |
| RE_PIN2 | GPIO 39 | Encoder B |
| ADC_PIN | GPIO 25 | Battery voltage |
| LCD_BLK | GPIO 4 | LCD backlight |
| LCD_CS | GPIO 15 | LCD chip select |
| AMP_PIN | GPIO 33 | Amplifier control |
| EN_1846 | GPIO 26 | A1846S power |
| EN_5807 | GPIO 27 | RDA5807 power |
| PWR_ON | GPIO 12 | MCU power control |
| SENB | GPIO 17 | A1846S chip select |
| S_SDA | GPIO 21 | I2C SDA |
| S_SCL | GPIO 22 | I2C SCL |

### Schematic Overview

```
┌─────────────────────────────────────────────────────────┐
│                      ESP32-WROOM                        │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐       │
│  │  RDA5807   │  │  A1846S    │  │ ST7789     │       │
│  │  (FM Radio)│  │(Transceiver│  │  (LCD)     │       │
│  └────────────┘  └────────────┘  └────────────┘       │
│         │               │               │               │
│         └───────────────┴───────────────┘               │
│                    I2C Bus                              │
└─────────────────────────────────────────────────────────┘
         │                    │
    ┌────┴────┐         ┌────┴────┐
    │  EC11   │         │ 18650   │
    │ Encoder │         │ Battery │
    └─────────┘         └─────────┘
```

---

## Building

### Prerequisites

- [PlatformIO](https://platformio.org/) extension for VSCode
- ESP32 development board
- USB cable for programming

### Build Commands

```bash
# Build the project
platformio run

# Upload to ESP32
platformio run --target upload

# Monitor serial output (115200 baud)
platformio device monitor -b 115200

# Build, upload and monitor in one command
platformio run --target upload && platformio device monitor -b 115200
```

### Dependencies

Dependencies are automatically installed by PlatformIO:

- `HamShield` - A1846S transceiver control
- `RDA5807` - FM radio receiver control
- `TFT_eSPI` - Display driver
- `OneButton` - Button debouncing and multi-click detection
- `RotaryEncoder` - Rotary encoder input handling
- `Battery18650Stats` - Battery level monitoring

---

## Usage

### Button Operations

| Button | Action | Function |
|--------|--------|----------|
| **UP (Click)** | Single | Jump to preset channel |
| **UP (Double)** | Double | Enter volume adjustment mode |
| **UP (Long)** | Long | Switch between FM Radio and Two-way mode |
| **UP (5x Click)** | 5 clicks | Calibrate frequency offset |
| **MIDDLE (Click)** | Single | Previous channel |
| **MIDDLE (Double)** | Double | Delete preset / Switch band |
| **MIDDLE (Long)** | Long | Save current frequency to preset |
| **DOWN (Press)** | Hold | Enable transmission (TX mode only) |
| **DOWN (Click)** | Single | Next channel / Enter RX mode |
| **DOWN (Double)** | Double | Toggle TX/RX mode |
| **ENCODER (Click)** | Single | Cycle step size |
| **ENCODER (Double)** | Double | Enter seek mode |
| **ENCODER (Long)** | Long | Power off |

### Display Elements

```
┌─────────────────────────────────────────────────────────┐
│  FM Radio                    409.800 MHz      Step:100KHz│
│  ┌──────────┐                                          │
│  │ STATIONS │  ● CH0: 409.750                          │
│  │          │  ○ CH1: 409.762                          │
│  │          │  ○ CH2: 409.775                          │
│  │          │  ○ CH3: 409.787                          │
│  │          │  ○ CH4: 409.800                          │
│  │          │  ○ CH5: 409.812                          │
│  └──────────┘                                          │
│                               SIGNAL: ████████ 80%     │
│       MODE: RX                VOL: ● 10    Batt: 85%   │
│                                                            │
│  ────────────────────────────────────────────────────── │
│  │     409.750    409.760    409.770    409.780      │
└─────────────────────────────────────────────────────────┘
```

### Frequency Bands

| Band | Range | Notes |
|------|-------|-------|
| WHF | 134-174 MHz | Weather band |
| VHF | 200-260 MHz | VHF civil band |
| UHF | 400-520 MHz | UHF civil band (default) |

---

## Safety Warning

<div align="center">

### ⚠️ REGULATORY COMPLIANCE NOTICE ⚠️

**This project is for educational purposes only.**

</div>

- **Do NOT transmit on frequencies without proper authorization**
- **Do NOT use this device in areas where it may interfere with emergency communications**
- **Do NOT transmit without a properly connected antenna**
- **Always comply with local radio regulations**

### Important Safety Notes

1. **Frequency Compliance**: The preset channels (409.750-409.987 MHz) are examples only. Verify legal frequencies in your region before use.

2. **RF Exposure**: Prolonged exposure to RF energy may be harmful. Maintain safe distance from antenna during transmission.

3. **Antenna Required**: **Never transmit without a properly matched antenna**. This can damage the RF power amplifier.

4. **Power Limits**: The firmware limits TX power to level 10 (out of 15) for safety. Do not modify this without proper understanding of RF safety.

5. **Legal Responsibility**: The user is solely responsible for ensuring compliance with all applicable laws and regulations.

---

## Troubleshooting

### Device won't power on
- Check battery voltage (should be >3.3V)
- Verify PWR_ON pin (GPIO 12) is not held low during boot (strapping pin)
- Try programming via USB first

### No audio output
- Check AMP_PIN (GPIO 33) is HIGH
- Verify volume is not muted (VOL > 0)
- Check speaker connections

### RF transmission not working
- Ensure you're in TX mode (double-click DOWN button)
- Verify frequency is within valid range
- Check antenna is connected
- Verify frequency offset calibration

### Display not working
- Check LCD_BLK pin (GPIO 4) is HIGH for backlight
- Verify LCD_CS pin (GPIO 15) connection
- Try changing rotation in code (line 131: `tft.setRotation(3)`)

### A1846S initialization failed
- Check I2C connections (SDA=21, SCL=22)
- Verify SENB pin (GPIO 17)
- Check power to EN_1846 (GPIO 26)
- Serial monitor will show retry attempts

---

## Project Structure

```
├── src/
│   └── esp32txshaoluceshichengg.ino    # Main firmware
├── lib/
│   ├── HamShield/                       # A1846S library
│   ├── RDA5807/                         # FM radio library
│   └── ...
├── platformio.ini                        # Build configuration
├── CLAUDE.md                            # Project documentation
└── README.md                            # This file
```

---

## Technical Specifications

| Specification | Value |
|---------------|-------|
| Supply Voltage | 3.7V (18650 battery) |
| Current Consumption | ~100mA (RX), ~500mA (TX) |
| RF Output Power | ~500mW (level 10) |
| Frequency Stability | ±2.5ppm |
| Audio Output | 1W into 8Ω |
| Display Resolution | 320x170 pixels |
| Battery Life | ~4 hours (TX), ~12 hours (RX) |

---

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the project
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

### Original Project

- **Original Author**: [gutao3800](https://oshwhub.com/gutao3800)
- **Original Project**: [Radio and Interphone (立创开源)](https://oshwhub.com/gutao3800/radio-and-interphone-lichuang-xi)
- This repository is a fork with code quality improvements and safety enhancements

### Libraries Used

- [HamShield](https://github.com/Whensomebodylovesyou/HamShield) - A1846S library
- [RDA5807](https://github.com/peakcreative/RDA5807) - FM radio library
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - Display library
- [OneButton](https://github.com/mathertel/OneButton) - Button library
- [RotaryEncoder](https://github.com/mathertel/RotaryEncoder) - Encoder library

---

## Author

**todaynews520**

- GitHub: [@todaynews520](https://github.com/todaynews520)

---

<div align="center">

  **⚠️ Use responsibly and legally ⚠️**

  Made with ❤️ for radio enthusiasts

  [Back to Top](#radiotalk---esp32-dual-mode-radio-device)

</div>
