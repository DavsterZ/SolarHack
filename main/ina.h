#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef struct {
	uint8_t i2c_addr;	// Direccion I2C del INA219 para ir cambiando
	float current_lsb;  // Registro CURRENT
	float power_lbs; 	// Registro POWER
} ina219_t;

// Inicia una instancia de INA219 con la direccion I2C dada
esp_err_t ina219_init(ina219_t *dev, uint8_t i2c_addr, float shunt_ohms, float max_current_A);

// Lee el voltaje de BUS en voltios (VIN- respecto GND)
esp_err_t ina219_read_bus_voltage(ina219_t *dev ,float *volts);

// Lee la corriente del registro CURRENT calibrado
esp_err_t ina219_read_current(ina219_t *dev, float *current_A);

// Lee la potencia del registro POWER calibrado
esp_err_t ina219_iread_power(ina219_t *dev, float *power_W);
