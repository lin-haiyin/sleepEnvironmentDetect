#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

namespace max30102_test {

struct Config {
    i2c_port_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    uint32_t clock_hz;
    uint8_t address;
};

struct VitalReading {
    float heart_rate_bpm;
    float spo2_percent;
    float quality;
    bool heart_rate_valid;
    bool spo2_valid;
    bool finger_present;
};

esp_err_t init_bus(const Config &config);
void scan_bus(const Config &config);
esp_err_t init(const Config &config);
void read_once();
VitalReading latest_vitals();
void log_wiring(const Config &config);

} // namespace max30102_test
