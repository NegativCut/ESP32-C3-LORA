# ESP32-C3 LoRa Morse Messenger

A wireless text communication system that encodes messages as Morse code and transmits them via LoRa radio at 433MHz.

## Block Diagram

See [TOOLS/ESP32C3_LORA.pdf](TOOLS/ESP32C3_LORA.pdf) for the full system block diagram.

## Hardware

- **MCU:** ESP32-C3 SUPERMINI with integrated Ra-02 LoRa module
- **Radio:** LoRa Ra-02 (433MHz) powered from ESP32 3.3V output
- **Display:** 20x4 I2C LCD (address 0x27)
- **Indicators:** TX and RX status LEDs (active-low)
- **Power:** 18650 battery with boost converter to 5V, or USB
- **Charging:** USB charger module with diode isolation

### Pin Connections

| Component | Pin |
|-----------|-----|
| LoRa CS | GPIO 7 |
| LoRa RST | GPIO 10 |
| LoRa DIO0 | GPIO 1 |
| TX LED | GPIO 2 (active-low) |
| RX LED | GPIO 3 (active-low) |
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |

## Features

- Bi-directional LoRa communication at 433MHz
- Real-time Morse encoding and decoding
- LCD display showing TX/RX messages and signal quality (RSSI/SNR)
- Non-blocking design for responsive operation
- Packet validation rejects noise/garbage (>10% invalid chars)
- Debug output via Serial (9600 baud) with raw hex dump

## Supported Characters

- **Letters:** A-Z (case insensitive)
- **Spaces:** Encoded as `/` in Morse

## Usage

1. Flash the firmware to your ESP32-C3
2. Open Serial Monitor at 9600 baud
3. Type a message and press Enter to transmit
4. Incoming messages are automatically decoded and displayed

### Debug Output

The serial monitor shows detailed debug information:
- Raw hex dump of received packets
- Morse encoding/decoding steps
- RSSI and SNR signal quality
- Packet validation results
- System statistics every 30 seconds

## Libraries Required

- [LoRa](https://github.com/sandeepmistry/arduino-LoRa) by Sandeep Mistry
- [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C) by Frank de Brabander

## LoRa Parameters

| Parameter | Value |
|-----------|-------|
| Frequency | 433 MHz |
| Spreading Factor | 7 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/5 |

Devices must use identical parameters to communicate.
