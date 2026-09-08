# Project Guidance

## Scope

This repository is a public ESP-IDF 5.5.x C++ reference for an ESP32-S3 sleep-environment sensing prototype. It contains no production credentials, broker addresses, private certificates, serial logs, or build artifacts.

## Hardware Rules

- `docs/HARDWARE.md` and `docs/HARDWARE.zh-CN.md` are the only public wiring references.
- Verify board silkscreen and module datasheets before changing GPIO assignments.
- Treat all MAX30102 readings as consumer-grade estimates, never medical measurements.
- Never connect a 5 V signal directly to an ESP32-S3 GPIO. WS2812B installations need common ground and, when powered at 5 V, an appropriate level shifter.

## Software Rules

- Keep device drivers in `components/<module>/`; `main/` only coordinates startup, test profiles, and periodic work.
- Keep Wi-Fi and MQTT values in the ignored `main/secrets.h`; copy `main/secrets.h.example` locally and replace placeholders.
- Preserve the MQTT command contract: RGB accepts `0/1/2/3`; WS2812B accepts `0/7/8/9`; `0` is global off.
- Build before hardware verification. A successful build is not evidence of a successful physical measurement.

## Public Contributions

- Do not commit real SSIDs, passwords, tokens, IP addresses, certificates, private keys, serial logs, or generated `sdkconfig` files.
- Update the matching English and Chinese documentation when public behavior, wiring, MQTT topics, or commands change.
