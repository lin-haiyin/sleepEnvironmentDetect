#pragma once

#include <cstdint>

#include "esp_err.h"

esp_err_t sensor_test_init();
void sensor_test_start_interactive_control();
void sensor_test_run_once();
void sensor_test_log_configuration();
const char *sensor_test_name();
uint32_t sensor_test_interval_ms();
