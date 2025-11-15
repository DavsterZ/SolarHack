#include "ina.h"

#include "driver/i2c.h"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"
#include "hal/touch_sens_types.h"
#include <stdint.h>

static const char *TAG = "INA219";

// Pines y configuracion de I2C
#define I2C_MASTER_SCL 22
#define I2C_MASTER_SDA 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0  // Como el ESP32 es el maestro no necesita tener buffer de transmision ni buffer de recibir datos
#define I2C_MASTER_RX_BUF_DISABLE 0  // Por eso se ponen a 0, si hubiera otro controlador mandandole datos se pondria el tamano del buffer

#define INA219_I2C_ADDR 0x40		// Esta es la direccion por default cuando solo se usa un INA es decir A0 y A1 = 0 

// Direcciones de registros
#define INA219_REG_CONFIG      0x00
#define INA219_REG_SHUNT_VOLT  0x01
#define INA219_REG_BUS_VOLT    0x02
#define INA219_REG_POWER       0x03
#define INA219_REG_CURRENT     0x04
#define INA219_REG_CALIB       0x05

// ------------- I2C ---------------
static esp_err_t i2c_master_init(void)
{
	i2c_config_t config = 
	{
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_MASTER_SDA,
		// Cuando nadie esta hablando por el cable I2C, lo mantiene en 1 (3V3) para que el ESP32 no vea basura
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = I2C_MASTER_SCL,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &config);
	if (err != ESP_OK)
		return err;
	
	// Inicia el i2c en modo master
	return i2c_driver_install(I2C_MASTER_NUM,
							  config.mode,
							  I2C_MASTER_RX_BUF_DISABLE,
							  I2C_MASTER_TX_BUF_DISABLE,
 							  0);	
}

// Escribir un valor en un registro
static esp_err_t ina219_write_reg(uint8_t reg, uint16_t value)
{
	// INA219 tiene 6 registros, cada registro son de 16 bits
	uint8_t data[3];
	data[0] = reg;
	data[1] = (uint8_t)(value >> 8);
	data[2] = (uint8_t)(value & 0xFF);
	
	// START -> direccion (0x40) -> byte de registro -> datos (MSB, LSB) -> STOP 
	return i2c_master_write_to_device(I2C_MASTER_NUM, 
									  INA219_I2C_ADDR,
									  data, 
									  sizeof(data),
									  pdMS_TO_TICKS(100));
}

static esp_err_t ina219_read_reg(uint8_t reg, uint16_t *value)
{
	uint8_t buf[2];
	esp_err_t err = i2c_master_read_from_device(I2C_MASTER_NUM,
												INA219_I2C_ADDR, 
												&reg, 
												1, 
												pdMS_TO_TICKS(100));
	if (err != ESP_OK)
		return err;
	
	*value = ((uint16_t)buf[0] << 8) | buf[1];
	return ESP_OK;
}

esp_err_t ina219_init(void)
{
	esp_err_t err = i2c_master_init();
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "Error inicializando I2C: %s", esp_err_to_name(err));
		return err;
	}

	// Resetea la configuracion de INA219 por si acaso de otros programas se ha quedado basura en el registro de configuracion
	// Este bit (15 a 1).
	err = ina219_write_reg(INA219_REG_CONFIG, 0x8000);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "Error haciendo reset al INA219: %s", esp_err_to_name(err));
		return err;
	}
	
	// Haciendo un delay para darle tiempo a hacer el reset
	vTaskDelay(pdMS_TO_TICKS(1));
	
	ESP_LOGI(TAG, "INA219 inicializado correctamente");
	return ESP_OK;
}

esp_err_t ina219_read_bus_voltage(float *volts)
{
	if (volts == NULL)
		return 	ESP_ERR_INVALID_ARG;

	uint16_t raw;
	esp_err_t err = ina219_read_reg(INA219_REG_BUS_VOLT, &raw);
	if (err != ESP_OK)
		return err;

	// Segun el datasheet:
	// volts = (BUS << 3) * 4 / 1000
	raw >>= 3;
	*volts = raw * (4 / 1000);

	return ESP_OK;
}




