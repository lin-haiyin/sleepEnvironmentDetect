#pragma once

#include "esp_err.h"
#include "mqtt_transport.hpp"

namespace network_manager {

struct Config {
    const char *wifi_ssid;
    const char *wifi_password;
    mqtt_transport::Config mqtt;
};

// Starts Wi-Fi station mode. MQTT is started asynchronously after GOT_IP.
// The function returns after Wi-Fi initialization/start, not after association.
esp_err_t start(const Config &config);
esp_err_t stop();
bool is_connected();

} // namespace network_manager
