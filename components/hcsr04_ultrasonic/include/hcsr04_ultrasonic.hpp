#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

namespace hcsr04_ultrasonic {

struct Config {
    gpio_num_t trig_gpio;
    gpio_num_t echo_gpio;
};

esp_err_t init(const Config &config);
void read_once();
void log_wiring(const Config &config);

} // namespace hcsr04_ultrasonic
