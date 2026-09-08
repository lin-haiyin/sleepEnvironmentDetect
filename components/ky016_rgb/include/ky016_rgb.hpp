#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "esp_err.h"

namespace ky016_rgb {

struct Config {
    gpio_num_t red_gpio;
    gpio_num_t green_gpio;
    gpio_num_t blue_gpio;
};

// Stable actuator contract for a future button, MQTT handler, or agent.
// 0 = off, 1 = red, 2 = green, 3 = blue. Invalid commands are rejected.
enum class Command : uint8_t {
    Off = 0,
    Red = 1,
    Green = 2,
    Blue = 3,
};

esp_err_t init(const Config &config);
esp_err_t set_command(uint8_t command);
const char *command_name(uint8_t command);
void run_once();
void log_wiring(const Config &config);

} // namespace ky016_rgb
