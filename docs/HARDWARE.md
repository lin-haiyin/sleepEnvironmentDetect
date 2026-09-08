# Hardware Guide

[English](HARDWARE.md) | [简体中文](HARDWARE.zh-CN.md)

## Before connecting power

- Verify every board pin against the actual ESP32-S3 board silkscreen. The ESP32-S3 chip datasheet confirms GPIO capabilities but not which pins a third-party board exposes.
- All modules must share ground.
- ESP32-S3 GPIO is 3.3 V logic. Never drive a GPIO with 5 V.
- Disconnect USB power before changing wiring.
- Test one newly attached module at a time.

## Active integrated configuration

| Module | Module pin | ESP32-S3 connection | Interface | Status / notes |
| --- | --- | --- | --- | --- |
| MAX30102 / compatible carrier | VCC/VIN | 3V3 only after carrier-board compatibility is confirmed | Power | Never connect 3V3 directly to the bare MAX30102 `VDD` pin |
| MAX30102 / compatible carrier | GND | GND | Power | Common ground |
| MAX30102 / compatible carrier | SDA | GPIO8 | I2C0 | Address `0x57`, 400 kHz |
| MAX30102 / compatible carrier | SCL | GPIO9 | I2C0 | Confirm carrier pull-ups and 3.3 V logic |
| DHT11 module | VCC | 3V3 | Power | Use a module with a data pull-up, or add one as required |
| DHT11 module | GND | GND | Power | Common ground |
| DHT11 module | DATA | GPIO4 | Single-wire | Read every two seconds |
| KY-018 | VCC | 3V3 | Power | Raw ADC result only, not lux |
| KY-018 | GND | GND | Power | Common ground |
| KY-018 | AO | GPIO1 / ADC1_CH0 | ADC | Confirm the actual module pin labels |
| KY-016 RGB LED | R/G/B | GPIO10 / GPIO11 / GPIO12 | GPIO output | Verify common-anode/common-cathode wiring and use current limiting |
| KY-016 RGB LED | GND or common | GND or 3V3 as required by module topology | Power | Verify module topology before power-on |
| WS2812B strip | V | Stable 5 V supply | Power | Size the supply for the strip load |
| WS2812B strip | G | Common GND | Power | ESP32, strip, and level shifter share ground |
| WS2812B strip | DIN / S | GPIO6 via 330-470 ohm series resistor | RMT data | Connect only to the input end; not DOUT |

## MAX30102 caution

The bare MAX30102 IC has a 1.7-2.0 V `VDD` requirement. The four-wire connection above is only for a documented carrier board that accepts 3.3 V on `VIN/VCC` and presents 3.3 V-safe I2C. If the board revision or pin labels are uncertain, do not power it. First identify the carrier board and read its schematic or documentation.

## WS2812B reliability

For a 5 V strip, a 3.3 V ESP32 data signal may not meet the strip input threshold. Use a 74AHCT125, 74HCT125, or similar 5 V-compatible unidirectional level shifter; do not use the strip DOUT connector as an input. Add a 0.1 uF decoupling capacitor at the level shifter and a 470-1000 uF capacitor near the strip power input. A BSS138 I2C level-shifter board is not a recommended substitute for this data line.

## Bring-up order

1. Run an I2C scan and MAX30102 profile; confirm `0x57`, part ID, and changing FIFO values with a finger present.
2. Run DHT11; validate plausible temperature and `%RH` readings at a two-second interval.
3. Run KY-018; verify that raw ADC values change when light changes.
4. Run KY-016; verify each color individually.
5. Run WS2812B with a known-safe 5 V supply and level-shifted DIN.
6. Switch to `Integrated` only after each module has passed its own observable check.

Historical KY-028, KY-037, HC-SR04, and C4002 components are retained for reference and isolated testing, but are not part of the active five-module product configuration.
