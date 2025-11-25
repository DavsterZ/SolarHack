#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef struct {
	uint8_t i2c_addr;	// Direccion I2C del INA219 para ir cambiando
} ina219_t;

// Inicia una instancia de INA219 con la direccion I2C dada
esp_err_t ina219_init(ina219_t *dev, uint8_t i2c_addr);

// Lee el voltaje de BUS en voltios (VIN- respecto GND)
esp_err_t ina219_read_bus_voltage(ina219_t *dev ,float *volts);
