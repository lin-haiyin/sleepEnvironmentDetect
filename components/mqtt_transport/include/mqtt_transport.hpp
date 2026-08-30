#pragma once

#include "esp_err.h"

namespace mqtt_transport {

struct Config {
    const char *broker_uri;   // mqtts://host:8883 or mqtt://host:1883
    const char *device_id;    // e.g. env-s3-01
    const char *username;     // nullptr when broker uses no username
    const char *password;     // nullptr when broker uses no password
    const char *root_ca_pem;  // required for mqtts; nullptr for non-TLS
};

// Starts the MQTT client. Wi-Fi must already be connected. This transport is
// intentionally not started by the current single-sensor bring-up firmware.
esp_err_t start(const Config &config);
esp_err_t stop();

// Returns whether esp-mqtt has been started. It may still be connecting.
bool is_started();
const char *device_id();

// Publishes a relative topic below device_id, e.g. "sensor/temp". The caller
// must provide a JSON payload; returns -1 when the client is not started.
int publish(const char *relative_topic, const char *payload, int qos = 0, bool retain = false);

// Convenience API for sensor/application code. The payload schema is:
// {"value":25.0,"unit":"C","valid":true,"ts_ms":12345}
// When valid=false, value is still emitted as a numeric placeholder and the
// consumer must use valid/error semantics instead of treating it as a reading.
int publish_measurement(const char *relative_topic, double value, const char *unit,
                        bool valid, const char *error = nullptr, int qos = 0,
                        bool retain = false);

} // namespace mqtt_transport
