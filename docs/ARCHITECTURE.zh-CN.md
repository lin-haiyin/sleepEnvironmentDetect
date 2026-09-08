# 系统架构

[English](ARCHITECTURE.md) | [简体中文](ARCHITECTURE.zh-CN.md)

## 设计目标

工程将板级接线、传感器驱动、设备控制和网络传输分层。每个硬件模块都是可独立构建的组件，可先切换为单模块测试，再进入整合运行时。

```text
组件驱动 -> 类型化读数 -> 整合采样编排 -> MQTT 传输层
                                  |                 |
                                  v                 v
                             本地串口控制台       应用层 Broker
                                  |
                                  v
                            device_app 命令路由
                                  |
                                  v
                         KY-016 三色灯 / WS2812B 灯带
```

## 运行层次

| 层 | 位置 | 职责 |
| --- | --- | --- |
| 板级配置 | `include/app_config.h` | GPIO、总线、活动模式与时序 |
| 驱动层 | `components/*` | 传感器/执行器初始化、采样与底层控制 |
| 整合层 | `main/sensor_test_runner.cpp` | 模式选择、周期采样、无效数据处理 |
| 命令边界 | `components/device_app` | 与主题无关的执行器路由和回执生成 |
| 联网层 | `components/network_manager`、`components/mqtt_transport` | Wi-Fi 生命周期、MQTT 重连、遥测、LWT、订阅 |
| 入口 | `main/main.cpp` | 启动日志、初始化顺序与非阻塞调度 |

## 整合模式

`SensorTestMode::Integrated` 是默认模式，会分别初始化 DHT11、KY-018、MAX30102、KY-016 和 WS2812B。某个模块失败时会记录错误，并在适用时发布无效数据；其余模块不会因此被故意阻断。

调度周期为 100 ms：灯带动画按此频率刷新，MAX30102 FIFO 持续读取，DHT11、光敏、心率和血氧的汇总遥测每 20 个 tick 发布一次，约为两秒一次。

## 单模块隔离

`SensorTestMode` 同时提供 I2C 扫描和各个支持/历史模块的独立模式。供电、接线、总线或时序出现问题时应优先使用这些模式。构建成功不能代表硬件成功，每个模块都要记录串口日志和可观察到的实物结果。

## 网络行为

仓库有意不包含 `main/secrets.h`。当本地存在被忽略的该文件时，固件会在本地传感器/灯光初始化后启动 Wi-Fi Station。`network_manager` 在 `IP_EVENT_STA_GOT_IP` 后启动 `mqtt_transport`；网络失败不会停止本地采集或灯光控制。

MQTT 完整主题只保留在传输组件中。驱动通过 `publish_measurement()` 发布相对主题，`device_app` 把 `actuator/*/set` 命令映射到硬件调用，因此应用端不依赖 GPIO 编号。

## 限制与非目标

- MAX30102 估算仅用于探索，不具备医疗用途。
- KY-018 输出 ADC 计数，不是 lux。
- 当前产品范围不包含历史 KY-028、KY-037、HC-SR04 和 C4002 硬件。
- 音频、OTA、云存储和显示功能默认关闭。
