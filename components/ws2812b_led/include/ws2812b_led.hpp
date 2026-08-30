#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"

namespace ws2812b_led {

struct Config {
    gpio_num_t data_gpio;
    uint32_t led_count;
};

esp_err_t init(const Config &config);
// 0=off, 1=warm-white breathing, 2=blue breathing, 3=marquee,
// 4=white, 5=chase, 6=rainbow, 7=auto cycle.
esp_err_t set_command(uint8_t command);
const char *command_name(uint8_t command);
void run_once();
void log_wiring(const Config &config);

} // namespace ws2812b_led
