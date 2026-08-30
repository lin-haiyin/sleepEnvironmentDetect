#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

namespace dht11_sensor {

struct Config {
    gpio_num_t data_gpio;
};

struct Reading {
    float temperature_c;
    float humidity_rh;
};

esp_err_t init(const Config &config);
void read_once();
esp_err_t read(Reading *reading);
void log_wiring(const Config &config, unsigned interval_ms);

} // namespace dht11_sensor
