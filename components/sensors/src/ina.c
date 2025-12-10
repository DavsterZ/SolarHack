#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#include "protect.h"
#include "ina.h"
#include "battery.h"

#include "driver/i2c.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"

#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "INA219";

#define INA_PANEL_ADDR   CONFIG_INA_ADDR_PANEL
#define INA_BAT_ADDR     CONFIG_INA_ADDR_BAT
#define INA_RSHUNT_OHMS  strtof(CONFIG_SHUNT_RESISTOR_OHM, NULL)
#define INA_PANEL_IMAX_A strtof(CONFIG_MAX_CURRENT_PANEL_A, NULL)
#define INA_BAT_IMAX_A   strtof(CONFIG_MAX_CURRENT_BAT_A, NULL)

// Configuracion de I2C
#define I2C_MASTER_NUM I2C_NUM_0


// Direcciones de registros
#define INA219_REG_CONFIG      0x00
#define INA219_REG_SHUNT_VOLT  0x01
#define INA219_REG_BUS_VOLT    0x02
#define INA219_REG_POWER       0x03
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIB       0x05

#define BAT_CAPACITY_AH  strtof(CONFIG_BAT_CAPACITY_AH, NULL)

ina219_data_t g_ina219_data[INA219_DEVICE_MAX];
float g_battery_soc = 50.0f;

// Escribir un valor en un registro
static esp_err_t ina219_write_reg(ina219_t *dev ,uint8_t reg, uint16_t value)
{
	// INA219 tiene 6 registros, cada registro son de 16 bits
	uint8_t data[3];
	data[0] = reg;
	data[1] = (uint8_t)(value >> 8);
	data[2] = (uint8_t)(value & 0xFF);
	
	// START -> direccion (0x40) -> byte de registro -> datos (MSB, LSB) -> STOP 
	return i2c_master_write_to_device(I2C_MASTER_NUM, 
									  dev->i2c_addr,
									  data, 
									  sizeof(data),
									  pdMS_TO_TICKS(100));
}

static esp_err_t ina219_read_reg(ina219_t *dev ,uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];

    // Primero escribir el registro que quieres leer
    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM,
        dev->i2c_addr,
        &reg, 
		1,        // escribir 1 byte: dirección del registro
        buf, 
		sizeof(buf),
        pdMS_TO_TICKS(100)
    );

    if (err != ESP_OK)
        return err;

    // Combinar MSB y LSB
    *value = ((uint16_t)buf[0] << 8) | buf[1];

    return ESP_OK;
}

static esp_err_t ina219_init(ina219_t *dev, uint8_t i2c_addr, float shunt_ohms, float max_current_A)
{
	if (!dev) return ESP_ERR_INVALID_ARG;
	dev->i2c_addr = i2c_addr;

	// Resetea la configuracion de INA219 por si acaso de otros programas se ha quedado basura en el registro de configuracion
	// Este bit (15 a 1).
	esp_err_t err = ina219_write_reg(dev, INA219_REG_CONFIG, 0x8000);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "Error haciendo reset al INA219(0x%02X): %s",dev->i2c_addr, esp_err_to_name(err));
		return err;
	}
	
	// Haciendo un delay para darle tiempo a hacer el reset
	vTaskDelay(pdMS_TO_TICKS(2));

	// Current_LSB = Imax / 32767
	float current_lsb = max_current_A / 32767.0f;
	dev->current_lsb = current_lsb;
	dev->power_lbs = 20.0f * current_lsb;

	// Calibracion: Cal = trunc(0.04096 / (Current_LSB * Rshunt))
	float calib_f = 0.04096f / (current_lsb * shunt_ohms);
	if (calib_f > 65535.0f) calib_f = 65535.0f;
	uint16_t calib = (uint16_t)(calib_f + 0.5f);
	
	err = ina219_write_reg(dev, INA219_REG_CALIB, calib);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "Error escribiendo CALIB en INA219(0x%02X): %s",dev->i2c_addr, esp_err_to_name(err));
		return err;
	}
	
	
	ESP_LOGI(TAG, "INA219(0x%02X) calibrado: Rshunt=%.3fΩ, Imax=%.2fA, I_LSB=%.6fA, CAL=0x%04X", dev->i2c_addr, shunt_ohms, max_current_A, current_lsb, calib);
	return ESP_OK;
}

static esp_err_t ina219_read_all(ina219_t *dev, float *v, float *i, float *p)
{
    uint16_t raw_v, raw_i, raw_p;
    
    // Lectura en bloque (o secuencial robusta)
    if (ina219_read_reg(dev, INA219_REG_BUS_VOLT, &raw_v) != ESP_OK) return ESP_FAIL;
    if (ina219_read_reg(dev, INA219_REG_CURRENT, &raw_i) != ESP_OK) return ESP_FAIL;
    if (ina219_read_reg(dev, INA219_REG_POWER, &raw_p) != ESP_OK) return ESP_FAIL;

    // Conversiones
    *v = (float)(raw_v >> 3) * 0.004f;
    *i = (float)((int16_t)raw_i) * dev->current_lsb;
    *p = (float)raw_p * dev->power_lbs;

    return ESP_OK;
}

static esp_err_t ina219_read_bus_voltage(ina219_t *dev, float *volts)
{
	if (!dev || !volts)
		return 	ESP_ERR_INVALID_ARG;

	uint16_t raw;
	esp_err_t err = ina219_read_reg(dev, INA219_REG_BUS_VOLT, &raw);
	if (err != ESP_OK)
		return err;

	// Segun el datasheet:
	// volts = (BUS << 3) * 4 / 1000
	raw >>= 3;
	*volts = (float)raw * (4.0 / 1000);

	return ESP_OK;
}

static esp_err_t ina219_read_current(ina219_t *dev, float *current_A)
{
	if (!dev || !current_A)
		return 	ESP_ERR_INVALID_ARG;

	uint16_t raw_u;
	esp_err_t err = ina219_read_reg(dev, INA219_REG_CURRENT, &raw_u);
	if (err != ESP_OK)
		return err;

	int16_t raw_a = (int16_t)raw_u;
	*current_A = (float)raw_a * dev->current_lsb;

	return ESP_OK;
}

static esp_err_t ina219_read_power(ina219_t *dev, float *power_W)
{
	if (!dev || !power_W)
		return 	ESP_ERR_INVALID_ARG;

	uint16_t raw_w;
	esp_err_t err = ina219_read_reg(dev, INA219_REG_POWER, &raw_w);
	if (err != ESP_OK)
		return err;

	*power_W = (float)raw_w * dev->power_lbs;

	return ESP_OK;
}

void ina_task(void *pvParameters) 
{
	(void) pvParameters;

	ina219_t dev_panel;
	ina219_t dev_battery;

	// Contadores de fallos
    int fail_count[INA219_DEVICE_MAX] = {0, 0};
    const int MAX_FAILURES = 10;

	ESP_ERROR_CHECK(ina219_init(&dev_panel, INA_PANEL_ADDR, INA_RSHUNT_OHMS, INA_PANEL_IMAX_A));
	ESP_ERROR_CHECK(ina219_init(&dev_battery, INA_BAT_ADDR, INA_RSHUNT_OHMS, INA_BAT_IMAX_A));

	ina219_t* devices[INA219_DEVICE_MAX] = { &dev_panel, &dev_battery };

	// Estimación inicial del SoC
    float v_start = 0, i_dum, p_dum;
    if(ina219_read_all(&dev_battery, &v_start, &i_dum, &p_dum) == ESP_OK && v_start > 1.0f) {
        g_battery_soc = battery_porcent_from_voltage(v_start);
        ESP_LOGI(TAG, "SoC inicial (por voltaje): %.1f%%", g_battery_soc);
    }

	while(1) {
		// Variables locales para almacenar lecturas temporalmente
        ina219_data_t local_data[INA219_DEVICE_MAX];
        bool read_ok[INA219_DEVICE_MAX] = {false, false};

		for (int i = 0; i < INA219_DEVICE_MAX; i++) {
			if (fail_count[i] > MAX_FAILURES) {
                // Podríamos intentar reinicializar aquí
                // ina219_init(devices[i], ...);
                fail_count[i] = 0; // Reset counter para reintentar
                ESP_LOGW(TAG, "Reintentando sensor INA %d tras fallos...", i);
			}

            // Leer sensores
            if(ina219_read_all(devices[i], 
                               &local_data[i].bus_voltage_V, 
                               &local_data[i].current_A, 
                               &local_data[i].power_W) == ESP_OK) 
            {
                read_ok[i] = true;
                fail_count[i] = 0;
            } else {
                read_ok[i] = false;
                fail_count[i]++;
                // Solo loguear error de vez en cuando para no saturar
                if(fail_count[i] == 1) ESP_LOGW(TAG, "Fallo lectura INA %d", i);
            }
        }

		// Robustez 2.C: Actualización del SoC aquí (cada 1s preciso)
        if (read_ok[INA219_DEVICE_BATTERY]) {
            // Asumiendo que esta tarea corre cada 1000ms
            g_battery_soc = battery_soc_update(
                g_battery_soc, 
                local_data[INA219_DEVICE_BATTERY].bus_voltage_V, 
                local_data[INA219_DEVICE_BATTERY].current_A, 
                1.0f, // dt = 1 segundo exacto
                BAT_CAPACITY_AH
            );
        }

		// Tomar Mutex UNA sola vez para actualizar todo
		if(g_data_mutex != NULL) {
			if(xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
				for(int i=0; i<INA219_DEVICE_MAX; i++) {
                    if(read_ok[i]) {
                        g_ina219_data[i] = local_data[i];
                    }
                }
				xSemaphoreGive(g_data_mutex);
			}
		}

		vTaskDelay(pdMS_TO_TICKS(CONFIG_TASK_INA_PERIOD_MS));
	}
}




