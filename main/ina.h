#pragma once

#include "esp_err.h"

esp_err_t ina219_init(void);

// Lee el voltaje de BUS en voltios (VIN- respecto GND)
esp_err_t ina219_read_bus_voltage(float *volts);