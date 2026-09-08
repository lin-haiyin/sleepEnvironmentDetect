#include "hcsr04_ultrasonic.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#include <cinttypes>

namespace hcsr04_ultrasonic {
namespace {
constexpr char TAG[] = "hcsr04";
constexpr int64_t WAIT_RISE_TIMEOUT_US = 30000;
constexpr int64_t WAIT_FALL_TIMEOUT_US = 30000;
constexpr int64_t MIN_PULSE_US = 100;
constexpr int64_t MAX_PULSE_US = 25000;
gpio_num_t s_trig_gpio = GPIO_NUM_NC;
gpio_num_t s_echo_gpio = GPIO_NUM_NC;

bool wait_for_level(int expected_level, int64_t timeout_us) {
    const int64_t deadline = esp_timer_get_time() + timeout_us;
    while (gpio_get_level(s_echo_gpio) != expected_level) {
        if (esp_timer_get_time() >= deadline) return false;
    }
    return true;
}
} // namespace

esp_err_t init(const Config &config) {
    s_trig_gpio = config.trig_gpio;
    s_echo_gpio = config.echo_gpio;

    gpio_config_t trig{};
    trig.pin_bit_mask = 1ULL << s_trig_gpio;
    trig.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&trig), TAG, "TRIG GPIO init failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(s_trig_gpio, 0), TAG, "TRIG idle level failed");

    gpio_config_t echo{};
    echo.pin_bit_mask = 1ULL << s_echo_gpio;
    echo.mode = GPIO_MODE_INPUT;
    echo.pull_up_en = GPIO_PULLUP_DISABLE;
    echo.pull_down_en = GPIO_PULLDOWN_DISABLE;
    return gpio_config(&echo);
}

void read_once() {
    gpio_set_level(s_trig_gpio, 0);
    esp_rom_delay_us(2);
    gpio_set_level(s_trig_gpio, 1);
    esp_rom_delay_us(10);
    gpio_set_level(s_trig_gpio, 0);

    if (!wait_for_level(1, WAIT_RISE_TIMEOUT_US)) {
        ESP_LOGW(TAG, "no ECHO rising edge within %" PRId64 " us; check target, wiring and 5V supply",
                 WAIT_RISE_TIMEOUT_US);
        return;
    }
    const int64_t pulse_start = esp_timer_get_time();
    if (!wait_for_level(0, WAIT_FALL_TIMEOUT_US)) {
        ESP_LOGW(TAG, "ECHO stayed high over %" PRId64 " us; check ECHO divider and range",
                 WAIT_FALL_TIMEOUT_US);
        return;
    }
    const int64_t pulse_us = esp_timer_get_time() - pulse_start;
    if (pulse_us < MIN_PULSE_US || pulse_us > MAX_PULSE_US) {
        ESP_LOGW(TAG, "invalid ECHO pulse=%" PRId64 " us", pulse_us);
        return;
    }

    // Distance = time * speed of sound / 2. 343 m/s -> 0.1715 mm/us.
    const int distance_mm = static_cast<int>((pulse_us * 343 + 1000) / 2000);
    ESP_LOGI(TAG, "echo=%" PRId64 " us distance=%d mm (%.1f cm)",
             pulse_us, distance_mm, distance_mm / 10.0F);
}

void log_wiring(const Config &config) {
    ESP_LOGI(TAG, "wiring: 5V->VCC GND->GND GPIO%d->TRIG", static_cast<int>(config.trig_gpio));
    ESP_LOGI(TAG, "ECHO(5V)->10k->GPIO%d node; GPIO%d node->20k->GND (required divider)",
             static_cast<int>(config.echo_gpio), static_cast<int>(config.echo_gpio));
}

} // namespace hcsr04_ultrasonic
