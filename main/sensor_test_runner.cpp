#include "sensor_drivers.h"

#include "app_config.h"
#include "c4002_radar.hpp"
#include "combined_console_control.hpp"
#include "dht11_sensor.hpp"
#include "device_app.hpp"
#include "hcsr04_ultrasonic.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "ky016_rgb.hpp"
#include "ky018_light.hpp"
#include "ky028_temperature.hpp"
#include "ky037_sound.hpp"
#include "max30102_test.hpp"
#include "mqtt_transport.hpp"
#include "rgb_console_control.hpp"
#include "ws2812b_led.hpp"
#include "ws2812_console_control.hpp"

namespace {

constexpr char TAG[] = "sensor_test";

constexpr max30102_test::Config MAX30102_CONFIG{
    envcfg::I2C_PORT, envcfg::I2C_SDA, envcfg::I2C_SCL,
    envcfg::I2C_HZ, envcfg::MAX30102_ADDRESS};
constexpr ky018_light::Config KY018_CONFIG{
    envcfg::ANALOG_ADC_UNIT, envcfg::LIGHT_ADC_CHANNEL};
constexpr ky028_temperature::Config KY028_CONFIG{
    envcfg::ANALOG_ADC_UNIT, envcfg::KY028_ADC_CHANNEL, envcfg::KY028_DO_GPIO};
constexpr dht11_sensor::Config DHT11_CONFIG{envcfg::DHT11_GPIO};
constexpr c4002_radar::Config C4002_CONFIG{
    envcfg::RADAR_UART, envcfg::RADAR_TX, envcfg::RADAR_RX, envcfg::RADAR_BAUD};
constexpr hcsr04_ultrasonic::Config HCSR04_CONFIG{
    envcfg::HCSR04_TRIG_GPIO, envcfg::HCSR04_ECHO_GPIO};
constexpr ky037_sound::Config KY037_CONFIG{
    envcfg::ANALOG_ADC_UNIT, envcfg::KY037_ADC_CHANNEL, envcfg::KY037_DO_GPIO};
constexpr ky016_rgb::Config KY016_CONFIG{
    envcfg::KY016_RED_GPIO, envcfg::KY016_GREEN_GPIO, envcfg::KY016_BLUE_GPIO};
constexpr ws2812b_led::Config WS2812B_CONFIG{
    envcfg::WS2812B_DATA_GPIO, envcfg::WS2812B_LED_COUNT};

uint32_t s_integrated_tick = 0;
bool s_dht_ready = false;
bool s_light_ready = false;
bool s_max30102_ready = false;
bool s_rgb_ready = false;
bool s_ws2812_ready = false;

esp_err_t init_integrated() {
    s_dht_ready = dht11_sensor::init(DHT11_CONFIG) == ESP_OK;
    if (!s_dht_ready) ESP_LOGE(TAG, "DHT11 init failed; its MQTT samples will be invalid");
    s_light_ready = ky018_light::init(KY018_CONFIG) == ESP_OK;
    if (!s_light_ready) ESP_LOGE(TAG, "KY018 init failed; its MQTT samples will be invalid");
    s_max30102_ready = max30102_test::init(MAX30102_CONFIG) == ESP_OK;
    if (!s_max30102_ready) ESP_LOGE(TAG, "MAX30102 init failed; its MQTT samples will be invalid");
    s_rgb_ready = ky016_rgb::init(KY016_CONFIG) == ESP_OK;
    if (!s_rgb_ready) ESP_LOGE(TAG, "KY-016 init failed; RGB commands will be rejected");
    s_ws2812_ready = ws2812b_led::init(WS2812B_CONFIG) == ESP_OK;
    if (!s_ws2812_ready) ESP_LOGE(TAG, "WS2812B init failed; LED commands will be rejected");
    s_integrated_tick = 0;
    ESP_LOGI(TAG, "integrated profile ready: DHT11=%s KY018=%s MAX30102=%s KY016=%s WS2812B=%s",
             s_dht_ready ? "yes" : "no", s_light_ready ? "yes" : "no",
             s_max30102_ready ? "yes" : "no", s_rgb_ready ? "yes" : "no",
             s_ws2812_ready ? "yes" : "no");
    return ESP_OK;
}

void publish_integrated_measurements() {
    dht11_sensor::Reading dht{};
    const esp_err_t dht_result = s_dht_ready ? dht11_sensor::read(&dht) : ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "sample temp=%.1f C humidity=%.1f %%RH valid=%s",
             dht.temperature_c, dht.humidity_rh, dht_result == ESP_OK ? "true" : "false");
    mqtt_transport::publish_measurement(device_app::topic::SENSOR_TEMP, dht.temperature_c, "C",
                                        dht_result == ESP_OK, dht_result == ESP_OK ? nullptr : "timeout");
    mqtt_transport::publish_measurement(device_app::topic::SENSOR_HUMIDITY, dht.humidity_rh, "%RH",
                                        dht_result == ESP_OK, dht_result == ESP_OK ? nullptr : "timeout");

    ky018_light::Reading light{};
    const esp_err_t light_result = s_light_ready ? ky018_light::read(&light) : ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "sample light_adc=%d valid=%s", light.average,
             light_result == ESP_OK ? "true" : "false");
    mqtt_transport::publish_measurement(device_app::topic::SENSOR_LIGHT, light.average, "adc_count",
                                        light_result == ESP_OK, light_result == ESP_OK ? nullptr : "unavailable");

    const max30102_test::VitalReading vitals = s_max30102_ready ? max30102_test::latest_vitals()
                                                                 : max30102_test::VitalReading{};
    ESP_LOGI(TAG, "sample heart_rate=%.1f bpm valid=%s spo2=%.1f %% valid=%s finger=%s",
             vitals.heart_rate_bpm, vitals.heart_rate_valid ? "true" : "false",
             vitals.spo2_percent, vitals.spo2_valid ? "true" : "false",
             vitals.finger_present ? "yes" : "no");
    const char *vital_error = !vitals.finger_present ? "finger_not_detected" :
                              (!vitals.heart_rate_valid || !vitals.spo2_valid) ?
                              "collecting_or_unstable" : nullptr;
    mqtt_transport::publish_measurement(device_app::topic::SENSOR_HEART_RATE, vitals.heart_rate_bpm, "bpm",
                                        vitals.heart_rate_valid, vital_error);
    mqtt_transport::publish_measurement(device_app::topic::SENSOR_SPO2, vitals.spo2_percent, "%",
                                        vitals.spo2_valid, vital_error);
}

} // namespace

const char *sensor_test_name() {
    using envcfg::SensorTestMode;
    switch (envcfg::ACTIVE_TEST) {
    case SensorTestMode::Integrated: return "INTEGRATED_ENVIRONMENT";
    case SensorTestMode::I2cScan: return "I2C_SCAN";
    case SensorTestMode::Max30102: return "MAX30102_RAW_AND_VITALS";
    case SensorTestMode::Ky018Light: return "KY018_LIGHT_ADC";
    case SensorTestMode::Ky028TemperatureModule: return "KY028_ADC_AND_THRESHOLD";
    case SensorTestMode::Dht11: return "DHT11";
    case SensorTestMode::C4002: return "C4002_UART";
    case SensorTestMode::HcSr04: return "HC_SR04_ULTRASONIC";
    case SensorTestMode::Ky037Sound: return "KY037_SOUND";
    case SensorTestMode::Ky016Rgb: return "KY016_RGB";
    case SensorTestMode::Ws2812b: return "WS2812B";
    }
    return "UNKNOWN";
}

uint32_t sensor_test_interval_ms() {
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::Integrated) return envcfg::INTEGRATED_INTERVAL_MS;
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::Dht11) return envcfg::DHT11_INTERVAL_MS;
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::HcSr04) return envcfg::HCSR04_INTERVAL_MS;
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::Ws2812b) return envcfg::WS2812B_INTERVAL_MS;
    return envcfg::TEST_INTERVAL_MS;
}

void sensor_test_log_configuration() {
    using envcfg::SensorTestMode;
    switch (envcfg::ACTIVE_TEST) {
    case SensorTestMode::Integrated:
        dht11_sensor::log_wiring(DHT11_CONFIG, envcfg::DHT11_INTERVAL_MS);
        ky018_light::log_wiring();
        max30102_test::log_wiring(MAX30102_CONFIG);
        ky016_rgb::log_wiring(KY016_CONFIG);
        ws2812b_led::log_wiring(WS2812B_CONFIG);
        break;
    case SensorTestMode::I2cScan:
    case SensorTestMode::Max30102: max30102_test::log_wiring(MAX30102_CONFIG); break;
    case SensorTestMode::Ky018Light: ky018_light::log_wiring(); break;
    case SensorTestMode::Ky028TemperatureModule: ky028_temperature::log_wiring(KY028_CONFIG); break;
    case SensorTestMode::Dht11:
        dht11_sensor::log_wiring(DHT11_CONFIG, envcfg::DHT11_INTERVAL_MS);
        break;
    case SensorTestMode::C4002: c4002_radar::log_wiring(C4002_CONFIG); break;
    case SensorTestMode::HcSr04: hcsr04_ultrasonic::log_wiring(HCSR04_CONFIG); break;
    case SensorTestMode::Ky037Sound: ky037_sound::log_wiring(KY037_CONFIG); break;
    case SensorTestMode::Ky016Rgb: ky016_rgb::log_wiring(KY016_CONFIG); break;
    case SensorTestMode::Ws2812b: ws2812b_led::log_wiring(WS2812B_CONFIG); break;
    }
}

esp_err_t sensor_test_init() {
    using envcfg::SensorTestMode;
    switch (envcfg::ACTIVE_TEST) {
    case SensorTestMode::Integrated: return init_integrated();
    case SensorTestMode::I2cScan: return max30102_test::init_bus(MAX30102_CONFIG);
    case SensorTestMode::Max30102: return max30102_test::init(MAX30102_CONFIG);
    case SensorTestMode::Ky018Light: return ky018_light::init(KY018_CONFIG);
    case SensorTestMode::Ky028TemperatureModule: return ky028_temperature::init(KY028_CONFIG);
    case SensorTestMode::Dht11: return dht11_sensor::init(DHT11_CONFIG);
    case SensorTestMode::C4002: return c4002_radar::init(C4002_CONFIG);
    case SensorTestMode::HcSr04: return hcsr04_ultrasonic::init(HCSR04_CONFIG);
    case SensorTestMode::Ky037Sound: return ky037_sound::init(KY037_CONFIG);
    case SensorTestMode::Ky016Rgb: return ky016_rgb::init(KY016_CONFIG);
    case SensorTestMode::Ws2812b: return ws2812b_led::init(WS2812B_CONFIG);
    }
    ESP_LOGE(TAG, "invalid active test");
    return ESP_ERR_INVALID_ARG;
}

void sensor_test_start_interactive_control() {
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::Integrated) {
        combined_console_control_start();
    }
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::Ky016Rgb) {
        rgb_console_control_start();
    }
    if (envcfg::ACTIVE_TEST == envcfg::SensorTestMode::Ws2812b) {
        ws2812_console_control_start();
    }
}

void sensor_test_run_once() {
    using envcfg::SensorTestMode;
    switch (envcfg::ACTIVE_TEST) {
    case SensorTestMode::Integrated:
        if (s_ws2812_ready) ws2812b_led::run_once();
        if (s_max30102_ready) max30102_test::read_once();
        ++s_integrated_tick;
        if ((s_integrated_tick % 20U) == 0U) publish_integrated_measurements();
        break;
    case SensorTestMode::I2cScan: max30102_test::scan_bus(MAX30102_CONFIG); break;
    case SensorTestMode::Max30102: max30102_test::read_once(); break;
    case SensorTestMode::Ky018Light: ky018_light::read_once(); break;
    case SensorTestMode::Ky028TemperatureModule: ky028_temperature::read_once(); break;
    case SensorTestMode::Dht11: dht11_sensor::read_once(); break;
    case SensorTestMode::C4002: c4002_radar::read_once(); break;
    case SensorTestMode::HcSr04: hcsr04_ultrasonic::read_once(); break;
    case SensorTestMode::Ky037Sound: ky037_sound::read_once(); break;
    case SensorTestMode::Ky016Rgb: break; // State changes only through the console command adapter.
    case SensorTestMode::Ws2812b: ws2812b_led::run_once(); break;
    }
}
