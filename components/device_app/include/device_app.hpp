#pragma once

#include <cstddef>

#include "esp_err.h"

namespace device_app {

// Relative MQTT-style routes. A future transport prepends the device ID, e.g.
// env-s3-01/sensor/temp and env-s3-01/actuator/rgb/set.
namespace topic {
constexpr char SENSOR_TEMP[] = "sensor/temp";
constexpr char SENSOR_HUMIDITY[] = "sensor/humidity";
constexpr char SENSOR_LIGHT[] = "sensor/light";
constexpr char SENSOR_HEART_RATE[] = "sensor/heart_rate";
constexpr char SENSOR_SPO2[] = "sensor/spo2";
constexpr char SENSOR_PRESENCE[] = "sensor/presence";
constexpr char SENSOR_DISTANCE[] = "sensor/distance";
constexpr char SENSOR_SOUND[] = "sensor/sound";
constexpr char RGB_SET[] = "actuator/rgb/set";
constexpr char RGB_STATE[] = "actuator/rgb/state";
constexpr char LED_SET[] = "actuator/led/set";
constexpr char LED_STATE[] = "actuator/led/state";
constexpr char STATUS[] = "status";
} // namespace topic

struct CommandReply {
    bool accepted;
    const char *topic;
    char payload[96];
};

// Transport-neutral command boundary. Current supported command:
// topic="actuator/rgb/set", payload="0".."3", or
// topic="actuator/led/set", payload="0", "7", "8" or "9" for WS2812B effects.
// The caller can publish the returned topic/payload as an MQTT acknowledgement.
esp_err_t handle_command(const char *topic, const char *payload, CommandReply *reply);

} // namespace device_app
