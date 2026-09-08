#include "ky016_rgb.hpp"

#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"

#include <cstdint>

namespace ky016_rgb {
namespace {
constexpr char TAG[] = "ky016";
constexpr uint32_t LOW_BRIGHTNESS_DUTY = 32;
uint8_t s_command = static_cast<uint8_t>(Command::Off);

esp_err_t apply_command(uint8_t command) {
    if (command > static_cast<uint8_t>(Command::Blue)) return ESP_ERR_INVALID_ARG;

    const uint32_t duties[4][3] = {
        {0, 0, 0},
        {LOW_BRIGHTNESS_DUTY, 0, 0},
        {0, LOW_BRIGHTNESS_DUTY, 0},
        {0, 0, LOW_BRIGHTNESS_DUTY},
    };
    for (int channel = 0; channel < 3; ++channel) {
        ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE,
                                          static_cast<ledc_channel_t>(channel),
                                          duties[command][channel]),
                            TAG, "set duty failed");
        ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE,
                                             static_cast<ledc_channel_t>(channel)),
                            TAG, "update duty failed");
    }
    s_command = command;
    return ESP_OK;
}
}

esp_err_t init(const Config &config) {
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_8_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 1000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer");
    const gpio_num_t pins[] = {config.red_gpio, config.green_gpio, config.blue_gpio};
    for (int channel = 0; channel < 3; ++channel) {
        ledc_channel_config_t led{};
        led.gpio_num = pins[channel];
        led.speed_mode = LEDC_LOW_SPEED_MODE;
        led.channel = static_cast<ledc_channel_t>(channel);
        led.intr_type = LEDC_INTR_DISABLE;
        led.timer_sel = LEDC_TIMER_0;
        ESP_RETURN_ON_ERROR(ledc_channel_config(&led), TAG, "channel");
    }
    return apply_command(static_cast<uint8_t>(Command::Off));
}

esp_err_t set_command(uint8_t command) {
    const esp_err_t result = apply_command(command);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "command=%u state=%s", static_cast<unsigned>(command), command_name(command));
    } else {
        ESP_LOGE(TAG, "invalid command=%u; valid: 0=off 1=red 2=green 3=blue",
                 static_cast<unsigned>(command));
    }
    return result;
}

const char *command_name(uint8_t command) {
    static constexpr const char *NAMES[] = {"off", "red", "green", "blue"};
    return command <= static_cast<uint8_t>(Command::Blue) ? NAMES[command] : "invalid";
}

void run_once() {
    // The serial test adapter (and later MQTT) calls set_command() directly.
}

void log_wiring(const Config &config) {
    ESP_LOGI(TAG, "wiring: common cathode->GND R/G/B via resistors->GPIO%d/%d/%d",
             static_cast<int>(config.red_gpio), static_cast<int>(config.green_gpio),
             static_cast<int>(config.blue_gpio));
}

} // namespace ky016_rgb
