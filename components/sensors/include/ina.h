#pragma once

#include "esp_err.h"
#include <stdint.h>

typedef struct {
	uint8_t i2c_addr;	// Direccion I2C del INA219 para ir cambiando
	float current_lsb;  // Registro CURRENT
	float power_lbs; 	// Registro POWER
} ina219_t;

typedef struct {
    float bus_voltage_V;
    float current_A;
    float power_W;
} ina219_data_t;

typedef enum {
    INA219_DEVICE_PANEL = 0,
    INA219_DEVICE_BATTERY,
    INA219_DEVICE_MAX
} ina219_device_t;

extern ina219_data_t g_ina219_data[INA219_DEVICE_MAX];

extern float g_battery_soc;

void ina_task(void *pvParameters);


/* Inicia una instancia de INA219 con la direccion I2C dada
esp_err_t ina219_init(ina219_t *dev, uint8_t i2c_addr, float shunt_ohms, float max_current_A);

// Lee el voltaje de BUS en voltios (VIN- respecto GND)
esp_err_t ina219_read_bus_voltage(ina219_t *dev ,float *volts);

// Lee la corriente del registro CURRENT calibrado
esp_err_t ina219_read_current(ina219_t *dev, float *current_A);

// Lee la potencia del registro POWER calibrado
esp_err_t ina219_read_power(ina219_t *dev, float *power_W);*/

