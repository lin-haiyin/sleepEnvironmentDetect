# MQTT Application Contract

[English](MQTT_CONTRACT.md) | [简体中文](MQTT_CONTRACT.zh-CN.md)

## Connection model

Each device has its own configurable identifier. Replace `<device-id>` in the following examples with the value configured in your local `main/secrets.h`.

```text
<device-id>/<relative-topic>
```

Use MQTT over TLS in deployed systems. Broker URI, credentials, CA material, and device identifiers belong only in local ignored configuration or a secure provisioning system.

## Topics

| Relative topic | Direction | QoS | Retain | Payload |
| --- | --- | --- | --- | --- |
| `status` | device -> application | 1 | yes | `{"state":"online"}`; LWT publishes `offline` |
| `sensor/temp` | device -> application | 0 | no | DHT11 temperature in `C` |
| `sensor/humidity` | device -> application | 0 | no | DHT11 relative humidity in `%RH` |
| `sensor/light` | device -> application | 0 | no | KY-018 raw `adc_count`, not lux |
| `sensor/heart_rate` | device -> application | 0 | no | MAX30102 estimate in `bpm` |
| `sensor/spo2` | device -> application | 0 | no | MAX30102 estimate in `%` |
| `actuator/rgb/set` | application -> device | 1 | no | One character: `0`, `1`, `2`, `3` |
| `actuator/rgb/state` | device -> application | 1 | no | Command acknowledgement JSON |
| `actuator/led/set` | application -> device | 1 | no | One character: `0`, `7`, `8`, `9` |
| `actuator/led/state` | device -> application | 1 | no | Command acknowledgement JSON |

Applications normally subscribe to:

```text
<device-id>/status
<device-id>/sensor/#
<device-id>/actuator/+/state
```

## Telemetry schema

```json
{"value":25.0,"unit":"C","valid":true,"ts_ms":123456}
```

| Field | Meaning |
| --- | --- |
| `value` | Numeric measurement. Use it only when `valid` is `true`. |
| `unit` | Unit defined by the topic. |
| `valid` | Sampling and initialization status. Never infer failure from `value == 0`. |
| `ts_ms` | Device uptime in milliseconds, not Unix time. |
| `error` | Present only when `valid` is `false`. |

Invalid example:

```json
{"value":0,"unit":"%","valid":false,"error":"collecting_or_unstable","ts_ms":123456}
```

## Commands and acknowledgements

| Target | Commands |
| --- | --- |
| RGB LED | `0=off`, `1=red`, `2=green`, `3=blue` |
| WS2812B | `0=off`, `7=warm_breath`, `8=blue_breath`, `9=marquee` |

`0` is a global-off command. Sending it to either actuator topic attempts to turn off both lighting devices. Repeating any valid command is safe and idempotent.

Accepted response:

```json
{"accepted":true,"command":9,"state":"marquee"}
```

Rejected response:

```json
{"accepted":false,"reason":"payload_must_be_0_7_8_or_9"}
```

A successfully published MQTT message is not proof that the device executed it. Treat `accepted:true` on the matching state topic as the execution acknowledgement.

## Broker access policy

Use a unique client identity and ACL per device. A device should publish only its own telemetry, status, and actuator state; it should subscribe only to its own actuator set topics. Do not expose anonymous plaintext brokers to the public internet.
