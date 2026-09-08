# MQTT 应用层契约

[English](MQTT_CONTRACT.md) | [简体中文](MQTT_CONTRACT.zh-CN.md)

## 连接模型

每台设备都有可配置的设备标识。以下示例中的 `<device-id>` 应替换为本地 `main/secrets.h` 中配置的值。

```text
<device-id>/<relative-topic>
```

正式部署使用 MQTT over TLS。Broker URI、账号密码、CA 证书和设备标识只能存在于被忽略的本地配置或安全配网系统中。

## 主题

| 相对主题 | 方向 | QoS | Retain | 负载 |
| --- | --- | --- | --- | --- |
| `status` | 设备 -> 应用 | 1 | 是 | `{"state":"online"}`；LWT 发布 `offline` |
| `sensor/temp` | 设备 -> 应用 | 0 | 否 | DHT11 温度，单位 `C` |
| `sensor/humidity` | 设备 -> 应用 | 0 | 否 | DHT11 相对湿度，单位 `%RH` |
| `sensor/light` | 设备 -> 应用 | 0 | 否 | KY-018 原始 `adc_count`，不是 lux |
| `sensor/heart_rate` | 设备 -> 应用 | 0 | 否 | MAX30102 估算心率，单位 `bpm` |
| `sensor/spo2` | 设备 -> 应用 | 0 | 否 | MAX30102 估算血氧，单位 `%` |
| `actuator/rgb/set` | 应用 -> 设备 | 1 | 否 | 单字符 `0`、`1`、`2`、`3` |
| `actuator/rgb/state` | 设备 -> 应用 | 1 | 否 | 命令回执 JSON |
| `actuator/led/set` | 应用 -> 设备 | 1 | 否 | 单字符 `0`、`7`、`8`、`9` |
| `actuator/led/state` | 设备 -> 应用 | 1 | 否 | 命令回执 JSON |

应用通常订阅：

```text
<device-id>/status
<device-id>/sensor/#
<device-id>/actuator/+/state
```

## 遥测 JSON

```json
{"value":25.0,"unit":"C","valid":true,"ts_ms":123456}
```

| 字段 | 含义 |
| --- | --- |
| `value` | 数值，只能在 `valid=true` 时用于业务判断。 |
| `unit` | 由主题定义的单位。 |
| `valid` | 采样与初始化是否成功；不能以 `value == 0` 判断故障。 |
| `ts_ms` | 设备启动后的毫秒数，不是 Unix 时间。 |
| `error` | 仅在 `valid=false` 时存在。 |

无效数据示例：

```json
{"value":0,"unit":"%","valid":false,"error":"collecting_or_unstable","ts_ms":123456}
```

## 控制与回执

| 目标 | 命令 |
| --- | --- |
| 三色 LED | `0=off`、`1=red`、`2=green`、`3=blue` |
| WS2812B | `0=off`、`7=warm_breath`、`8=blue_breath`、`9=marquee` |

`0` 是统一熄灭命令。向任一执行器主题发送它，都会尝试关闭两类灯。重复发送任意合法命令是安全且幂等的。

成功回执：

```json
{"accepted":true,"command":9,"state":"marquee"}
```

拒绝回执：

```json
{"accepted":false,"reason":"payload_must_be_0_7_8_or_9"}
```

MQTT 发布函数成功只代表消息被客户端接受，不代表硬件已执行。应用应以对应状态主题上的 `accepted:true` 作为执行成功依据。

## Broker 权限

每台设备应使用独立身份和 ACL。设备只发布自己的遥测、状态和执行器回执，只订阅自己的执行器设定主题。不要将匿名明文 Broker 暴露到公网。
