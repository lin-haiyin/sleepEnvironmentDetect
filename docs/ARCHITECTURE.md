# Architecture

[English](ARCHITECTURE.md) | [简体中文](ARCHITECTURE.zh-CN.md)

## Design goals

The project separates board wiring, sensor drivers, application commands, and network transport. Each hardware module is independently buildable and can be selected as the active test profile before it joins the integrated runtime.

```text
component driver -> typed reading -> integration runner -> MQTT transport
                                            |                 |
                                            v                 v
                                     local console       application broker
                                            |
                                            v
                                    device_app command router
                                            |
                                            v
                                  KY-016 RGB / WS2812B strip
```

## Runtime layers

| Layer | Location | Responsibility |
| --- | --- | --- |
| Board configuration | `include/app_config.h` | GPIO assignments, bus settings, active profile, timing |
| Drivers | `components/*` | Sensor and actuator initialization, sampling, low-level control |
| Integration | `main/sensor_test_runner.cpp` | Profile selection, periodic measurement, invalid-data handling |
| Command boundary | `components/device_app` | Topic-independent actuator routing and acknowledgement payloads |
| Connectivity | `components/network_manager`, `components/mqtt_transport` | Wi-Fi station lifecycle, MQTT reconnect, telemetry, LWT, subscriptions |
| Entry point | `main/main.cpp` | Boot diagnostics, startup ordering, non-blocking scheduling |

## Integrated profile

`SensorTestMode::Integrated` is the default profile. It initializes DHT11, KY-018, MAX30102, KY-016, and WS2812B independently. A failed module is logged and publishes invalid data where applicable; it does not intentionally prevent the remaining modules from starting.

The scheduler runs every 100 ms. The strip animation is serviced on that cadence, MAX30102 FIFO is read continuously, and consolidated DHT11, light, heart-rate, and SpO2 telemetry is published every 20 ticks, approximately every two seconds.

## Profile isolation

`SensorTestMode` also provides I2C scan and individual profiles for all supported or historical components. Use those profiles when diagnosing power, wiring, bus, or timing problems. A single successful build does not verify physical hardware; record serial output and an observable test action for each module.

## Network behavior

`main/secrets.h` is intentionally absent from this repository. When a local ignored copy exists, the firmware starts Wi-Fi Station mode after local sensor/actuator initialization. `network_manager` starts `mqtt_transport` after `IP_EVENT_STA_GOT_IP`; a connectivity failure does not stop local sensing or lighting.

MQTT transport keeps full topics inside the transport component. Drivers publish relative topic suffixes through `publish_measurement()`, while `device_app` maps `actuator/*/set` commands to hardware-specific calls. This prevents application clients from depending on GPIO assignments.

## Constraints and non-goals

- MAX30102 estimation is exploratory and non-medical.
- KY-018 reports ADC counts, not lux.
- Current active product scope does not include historical KY-028, KY-037, HC-SR04, or C4002 hardware.
- Audio, OTA, cloud storage, and display features are deliberately not enabled by default.
