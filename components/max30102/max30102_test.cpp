#include "max30102_test.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace max30102_test {
namespace {

constexpr char TAG[] = "max30102";
constexpr int I2C_TIMEOUT_MS = 100;
constexpr uint8_t EXPECTED_PART_ID = 0x15;
constexpr size_t SAMPLE_WINDOW = 200;       // 8 seconds at the effective 25 Hz FIFO rate.
constexpr uint32_t FINGER_IR_THRESHOLD = 10000;
constexpr float EFFECTIVE_SAMPLE_RATE_HZ = 25.0F;
Config s_config{};
VitalReading s_latest_vitals{};
bool s_bus_ready = false;
std::array<uint32_t, SAMPLE_WINDOW> s_red_samples{};
std::array<uint32_t, SAMPLE_WINDOW> s_ir_samples{};
size_t s_sample_count = 0;
size_t s_write_index = 0;
uint8_t s_no_finger_samples = 0;

struct VitalEstimate {
    float heart_rate_bpm = 0.0F;
    float spo2_percent = 0.0F;
    float quality = 0.0F;
    float perfusion_index_percent = 0.0F;
    bool signal_available = false;
    bool heart_rate_valid = false;
    bool spo2_valid = false;
};

uint32_t sample_at(const std::array<uint32_t, SAMPLE_WINDOW> &samples, size_t index) {
    const size_t oldest = s_sample_count < SAMPLE_WINDOW ? 0 : s_write_index;
    return samples[(oldest + index) % SAMPLE_WINDOW];
}

void clear_signal_window() {
    s_sample_count = 0;
    s_write_index = 0;
}

void append_sample(uint32_t red, uint32_t ir) {
    if (ir < FINGER_IR_THRESHOLD) {
        if (s_no_finger_samples < 10) ++s_no_finger_samples;
        if (s_no_finger_samples >= 10) clear_signal_window();
        return;
    }

    s_no_finger_samples = 0;
    s_red_samples[s_write_index] = red;
    s_ir_samples[s_write_index] = ir;
    s_write_index = (s_write_index + 1) % SAMPLE_WINDOW;
    if (s_sample_count < SAMPLE_WINDOW) ++s_sample_count;
}

VitalEstimate calculate_vitals() {
    VitalEstimate estimate{};
    if (s_sample_count < SAMPLE_WINDOW) return estimate;

    double red_sum = 0.0;
    double ir_sum = 0.0;
    for (size_t i = 0; i < SAMPLE_WINDOW; ++i) {
        red_sum += sample_at(s_red_samples, i);
        ir_sum += sample_at(s_ir_samples, i);
    }
    const double red_mean = red_sum / SAMPLE_WINDOW;
    const double ir_mean = ir_sum / SAMPLE_WINDOW;
    if (red_mean <= 0.0 || ir_mean < FINGER_IR_THRESHOLD) return estimate;

    // Remove a linear trend before calculating AC RMS so slow finger movement does not
    // dominate the ratio-of-ratios estimate.
    const double center = (SAMPLE_WINDOW - 1) / 2.0;
    double time_energy = 0.0;
    double red_time = 0.0;
    double ir_time = 0.0;
    for (size_t i = 0; i < SAMPLE_WINDOW; ++i) {
        const double t = static_cast<double>(i) - center;
        time_energy += t * t;
        red_time += t * (sample_at(s_red_samples, i) - red_mean);
        ir_time += t * (sample_at(s_ir_samples, i) - ir_mean);
    }
    const double red_slope = red_time / time_energy;
    const double ir_slope = ir_time / time_energy;
    double red_energy = 0.0;
    double ir_energy = 0.0;
    for (size_t i = 0; i < SAMPLE_WINDOW; ++i) {
        const double t = static_cast<double>(i) - center;
        const double red_ac = sample_at(s_red_samples, i) - red_mean - red_slope * t;
        const double ir_ac = sample_at(s_ir_samples, i) - ir_mean - ir_slope * t;
        red_energy += red_ac * red_ac;
        ir_energy += ir_ac * ir_ac;
    }
    const double red_rms = std::sqrt(red_energy / SAMPLE_WINDOW);
    const double ir_rms = std::sqrt(ir_energy / SAMPLE_WINDOW);
    if (red_rms < 20.0 || ir_rms < 20.0) return estimate;

    estimate.signal_available = true;
    estimate.perfusion_index_percent = static_cast<float>(100.0 * ir_rms / ir_mean);

    // This empirical ratio-of-ratios curve is deliberately reported as an
    // uncalibrated consumer estimate.  Heart-rate quality must not suppress an
    // otherwise computable SpO2 candidate; each value has its own validity bit.
    const double ratio = (red_rms / red_mean) / (ir_rms / ir_mean);
    const double calculated_spo2 = 110.0 - 25.0 * ratio;
    estimate.spo2_percent = static_cast<float>(calculated_spo2);
    estimate.spo2_valid = ratio >= 0.2 && ratio <= 1.8 &&
                          calculated_spo2 >= 70.0 && calculated_spo2 <= 100.0 &&
                          estimate.perfusion_index_percent >= 0.05F &&
                          estimate.perfusion_index_percent <= 20.0F;

    // Autocorrelation of the first derivative rejects DC and much of the motion drift.
    constexpr int MIN_LAG = 8;  // 187.5 BPM at 25 Hz.
    constexpr int MAX_LAG = 33; // 45.5 BPM at 25 Hz.
    int best_lag = 0;
    double best_correlation = -1.0;
    std::array<double, MAX_LAG + 1> correlations{};
    for (int lag = MIN_LAG; lag <= MAX_LAG; ++lag) {
        double numerator = 0.0;
        double energy_a = 0.0;
        double energy_b = 0.0;
        for (size_t i = 1; i + static_cast<size_t>(lag) < SAMPLE_WINDOW; ++i) {
            const double a = static_cast<double>(sample_at(s_ir_samples, i)) -
                             static_cast<double>(sample_at(s_ir_samples, i - 1));
            const double b = static_cast<double>(sample_at(s_ir_samples, i + lag)) -
                             static_cast<double>(sample_at(s_ir_samples, i + lag - 1));
            numerator += a * b;
            energy_a += a * a;
            energy_b += b * b;
        }
        if (energy_a > 0.0 && energy_b > 0.0) {
            correlations[lag] = numerator / std::sqrt(energy_a * energy_b);
            if (correlations[lag] > best_correlation) {
                best_correlation = correlations[lag];
                best_lag = lag;
            }
        }
    }
    // Prefer the fundamental when the strongest peak is its first harmonic.
    if (best_lag > 0 && best_lag * 2 <= MAX_LAG &&
        correlations[best_lag * 2] > best_correlation * 0.82) {
        best_lag *= 2;
        best_correlation = correlations[best_lag];
    }

    estimate.quality = static_cast<float>(std::max(0.0, best_correlation));
    if (best_lag > 0) {
        const double calculated_bpm = 60.0 * EFFECTIVE_SAMPLE_RATE_HZ / best_lag;
        estimate.heart_rate_bpm = static_cast<float>(calculated_bpm);
        // Bring-up is intended for a resting finger measurement. Reject the
        // common short-lag/harmonic spike instead of presenting it as valid.
        estimate.heart_rate_valid = best_correlation >= 0.30 &&
                                    calculated_bpm >= 45.0 && calculated_bpm <= 150.0;
    }
    return estimate;
}

esp_err_t read_register(uint8_t reg, uint8_t *data, size_t length) {
    return i2c_master_write_read_device(s_config.port, s_config.address, &reg, 1, data,
                                        length, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

esp_err_t write_register(uint8_t reg, uint8_t value) {
    const uint8_t bytes[] = {reg, value};
    return i2c_master_write_to_device(s_config.port, s_config.address, bytes, sizeof(bytes),
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
}

bool probe(uint8_t address) {
    i2c_cmd_handle_t command = i2c_cmd_link_create();
    if (command == nullptr) return false;
    i2c_master_start(command);
    i2c_master_write_byte(command, static_cast<uint8_t>(address << 1U) | I2C_MASTER_WRITE, true);
    i2c_master_stop(command);
    const esp_err_t result = i2c_master_cmd_begin(
        s_config.port, command, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(command);
    return result == ESP_OK;
}

} // namespace

esp_err_t init_bus(const Config &config) {
    s_config = config;
    if (s_bus_ready) return ESP_OK;

    i2c_config_t bus{};
    bus.mode = I2C_MODE_MASTER;
    bus.sda_io_num = config.sda;
    bus.scl_io_num = config.scl;
    bus.sda_pullup_en = GPIO_PULLUP_ENABLE;
    bus.scl_pullup_en = GPIO_PULLUP_ENABLE;
    bus.master.clk_speed = config.clock_hz;
    ESP_RETURN_ON_ERROR(i2c_param_config(config.port, &bus), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(config.port, I2C_MODE_MASTER, 0, 0, 0),
                        TAG, "I2C driver install failed");
    s_bus_ready = true;
    return ESP_OK;
}

void scan_bus(const Config &config) {
    s_config = config;
    int count = 0;
    for (uint8_t address = 1; address < 0x7f; ++address) {
        if (probe(address)) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X%s", address,
                     address == config.address ? " (MAX30102 expected)" : "");
            ++count;
        }
    }
    if (count == 0) {
        ESP_LOGW(TAG, "I2C scan found no devices; check module power, GND, SDA and SCL");
    }
}

esp_err_t init(const Config &config) {
    ESP_LOGI(TAG, "step 1/5: start I2C port=%d SDA=GPIO%d SCL=GPIO%d clock=%" PRIu32 " Hz",
             static_cast<int>(config.port), static_cast<int>(config.sda),
             static_cast<int>(config.scl), config.clock_hz);
    ESP_RETURN_ON_ERROR(init_bus(config), TAG, "I2C init failed");

    ESP_LOGI(TAG, "step 2/5: scan I2C bus");
    scan_bus(config);

    uint8_t part_id = 0;
    uint8_t revision = 0;
    ESP_LOGI(TAG, "step 3/5: read IDs from address 0x%02X", config.address);
    ESP_RETURN_ON_ERROR(read_register(0xff, &part_id, 1), TAG, "device did not answer");
    ESP_RETURN_ON_ERROR(read_register(0xfe, &revision, 1), TAG, "revision read failed");
    ESP_LOGI(TAG, "Part ID=0x%02X Revision=0x%02X", part_id, revision);
    if (part_id != EXPECTED_PART_ID) {
        ESP_LOGE(TAG, "unexpected Part ID: expected=0x%02X actual=0x%02X",
                 EXPECTED_PART_ID, part_id);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "step 4/5: reset and configure sensor");
    ESP_RETURN_ON_ERROR(write_register(0x09, 0x40), TAG, "reset failed");
    for (int retry = 0; retry < 100; ++retry) {
        uint8_t mode = 0;
        ESP_RETURN_ON_ERROR(read_register(0x09, &mode, 1), TAG, "reset status failed");
        if ((mode & 0x40U) == 0) break;
        vTaskDelay(pdMS_TO_TICKS(1));
        if (retry == 99) return ESP_ERR_TIMEOUT;
    }

    // FIFO avg=4 + rollover; SpO2 mode; 4096 nA, 100 sps, 411 us; LEDs 19.0 mA.
    // Averaging four conversions makes the effective FIFO sample rate 25 Hz.
    const std::array<std::array<uint8_t, 2>, 9> registers{{
        {{0x08, 0x50}}, {{0x0a, 0x27}}, {{0x0c, 0x5f}}, {{0x0d, 0x5f}},
        {{0x04, 0}}, {{0x05, 0}}, {{0x06, 0}}, {{0x09, 0x03}}, {{0x02, 0}},
    }};
    for (const auto &entry : registers) {
        ESP_RETURN_ON_ERROR(write_register(entry[0], entry[1]), TAG, "configuration failed");
    }

    ESP_LOGI(TAG, "step 5/5: FIFO ready");
    clear_signal_window();
    s_no_finger_samples = 0;
    ESP_LOGI(TAG, "raw Red/IR + consumer HR/SpO2 estimate started; hold finger still for 8-10 s");
    return ESP_OK;
}

void read_once() {
    s_latest_vitals = VitalReading{0.0F, 0.0F, 0.0F, false, false, false};
    uint8_t pointers[3]{};
    esp_err_t result = read_register(0x04, pointers, sizeof(pointers));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "FIFO pointer read: %s", esp_err_to_name(result));
        return;
    }

    const uint8_t samples = static_cast<uint8_t>((pointers[0] - pointers[2]) & 0x1fU);
    if (samples == 0) {
        ESP_LOGW(TAG, "FIFO has no sample");
        return;
    }

    uint32_t red = 0;
    uint32_t ir = 0;
    uint32_t red_min = UINT32_MAX;
    uint32_t red_max = 0;
    uint32_t ir_min = UINT32_MAX;
    uint32_t ir_max = 0;
    uint64_t red_sum = 0;
    uint64_t ir_sum = 0;
    for (uint8_t sample = 0; sample < samples; ++sample) {
        uint8_t fifo[6]{};
        result = read_register(0x07, fifo, sizeof(fifo));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "FIFO read: %s", esp_err_to_name(result));
            return;
        }
        red = ((uint32_t{fifo[0]} << 16U) | (uint32_t{fifo[1]} << 8U) | fifo[2]) & 0x3ffffU;
        ir = ((uint32_t{fifo[3]} << 16U) | (uint32_t{fifo[4]} << 8U) | fifo[5]) & 0x3ffffU;
        red_min = std::min(red_min, red);
        red_max = std::max(red_max, red);
        ir_min = std::min(ir_min, ir);
        ir_max = std::max(ir_max, ir);
        red_sum += red;
        ir_sum += ir;
        append_sample(red, ir);
    }
    const uint32_t red_average = static_cast<uint32_t>(red_sum / samples);
    const uint32_t ir_average = static_cast<uint32_t>(ir_sum / samples);
    const bool finger_present = ir_average >= FINGER_IR_THRESHOLD;
    ESP_LOGI(TAG,
             "raw samples=%u red_avg=%" PRIu32 " range=%" PRIu32 "..%" PRIu32
             " ir_avg=%" PRIu32 " range=%" PRIu32 "..%" PRIu32 " contact=%s window=%u/%u",
             samples, red_average, red_min, red_max, ir_average, ir_min, ir_max,
             finger_present ? "yes" : "no",
             static_cast<unsigned>(s_sample_count), static_cast<unsigned>(SAMPLE_WINDOW));

    if (!finger_present) {
        ESP_LOGW(TAG, "vitals status=NO_FINGER ir_avg=%" PRIu32
                      " threshold=%" PRIu32 "; cover the optical window completely",
                 ir_average, FINGER_IR_THRESHOLD);
        return;
    }
    if (s_sample_count < SAMPLE_WINDOW) {
        ESP_LOGI(TAG, "vitals status=COLLECTING; keep finger still (%u%%)",
                 static_cast<unsigned>(s_sample_count * 100 / SAMPLE_WINDOW));
        return;
    }

    const VitalEstimate estimate = calculate_vitals();
    s_latest_vitals = VitalReading{estimate.heart_rate_bpm, estimate.spo2_percent,
                                    estimate.quality, estimate.heart_rate_valid,
                                    estimate.spo2_valid, true};
    if (!estimate.signal_available) {
        ESP_LOGW(TAG, "vitals status=SIGNAL_TOO_WEAK; keep finger still and block ambient light");
    } else if (estimate.heart_rate_valid && estimate.spo2_valid) {
        ESP_LOGI(TAG,
                 "vitals bpm=%.1f hr_valid=yes spo2=%.1f%% spo2_valid=yes quality=%.2f pi=%.2f%% "
                 "CONSUMER_ESTIMATE_NOT_MEDICAL",
                 estimate.heart_rate_bpm, estimate.spo2_percent, estimate.quality,
                 estimate.perfusion_index_percent);
    } else {
        ESP_LOGW(TAG,
                 "vitals status=SIGNAL_UNSTABLE bpm=%.1f hr_valid=%s spo2=%.1f%% spo2_valid=%s "
                 "quality=%.2f pi=%.2f%% CONSUMER_ESTIMATE_NOT_MEDICAL",
                 estimate.heart_rate_bpm, estimate.heart_rate_valid ? "yes" : "no",
                 estimate.spo2_percent, estimate.spo2_valid ? "yes" : "no",
                 estimate.quality, estimate.perfusion_index_percent);
    }
}

VitalReading latest_vitals() { return s_latest_vitals; }

void log_wiring(const Config &config) {
    ESP_LOGI(TAG, "wiring: verified 3.3V-compatible module VIN->3V3 GND->GND GPIO%d->SDA GPIO%d->SCL INT=NC",
             static_cast<int>(config.sda), static_cast<int>(config.scl));
    ESP_LOGI(TAG, "expected address=0x%02X Part ID=0x%02X", config.address, EXPECTED_PART_ID);
}

} // namespace max30102_test
