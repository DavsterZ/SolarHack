#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "driver/i2c.h"

#include "ina.h"
#include "adc.h"
#include "protect.h"
#include "battery.h"

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BAT_CAPACITY_AH  strtof(CONFIG_BAT_CAPACITY_AH, NULL)
#define LOOP_PERIOD_S    1.0f

// Configuracion Pines I2C
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL CONFIG_I2C_SCL_PIN
#define I2C_MASTER_SDA CONFIG_I2C_SDA_PIN
#define I2C_MASTER_FREQ_HZ CONFIG_I2C_FREQ_HZ
#define I2C_MASTER_TX_BUF_DISABLE 0  // Como el ESP32 es el maestro no necesita tener buffer de transmision ni buffer de recibir datos
#define I2C_MASTER_RX_BUF_DISABLE 0  // Por eso se ponen a 0, si hubiera otro controlador mandandole datos se pondria el tamano del buffer

SemaphoreHandle_t g_data_mutex = NULL;


static const char *TAG = "MAIN";

// Configurar Init I2C
static esp_err_t i2c_master_init(void)
{
	static bool initialized = false;
	if (initialized) return ESP_OK;

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
	err = i2c_driver_install(I2C_MASTER_NUM,
							  config.mode,
							  I2C_MASTER_RX_BUF_DISABLE,
							  I2C_MASTER_TX_BUF_DISABLE,
 							  0);	

	if (err == ESP_OK)
		initialized = true;

	return err;
}

void app_main(void)
{	
	// Creamos el Mutex
    g_data_mutex = xSemaphoreCreateMutex();
    if (g_data_mutex == NULL) {
        ESP_LOGE(TAG, "No se pudo crear el Mutex");
        return;
    }

	i2c_master_init();
	
	xTaskCreate(ina_task, "ina_task", 4096, NULL, 5, NULL);
    xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);

	vTaskDelay(pdMS_TO_TICKS(1500));

	float soc = 50.0f;
	float v_bat_init = 0.0f;

	if(xSemaphoreTake(g_data_mutex, portMAX_DELAY)) {
		v_bat_init = g_ina219_data[INA219_DEVICE_BATTERY].bus_voltage_V;
		xSemaphoreGive(g_data_mutex);
	}

    if (v_bat_init > 1.0f) {
        soc = battery_porcent_from_voltage(v_bat_init);
        ESP_LOGI(TAG, "SoC inicial estimado: Vbat=%.3f V -> %.1f%%",
                 v_bat_init, soc);
    } else {
        ESP_LOGW(TAG, "No se pudo leer Vbat inicial; usando SoC=50%%");
    }


    while (1) {
		float v_entrada=0, i_entrada=0, w_entrada=0;
        float v_salida=0, i_salida=0, w_salida=0;
        float ldr_r[LDR_COUNT] = {0};

		if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
			v_entrada = g_ina219_data[INA219_DEVICE_PANEL].bus_voltage_V;
	        i_entrada = g_ina219_data[INA219_DEVICE_PANEL].current_A;
	        w_entrada = g_ina219_data[INA219_DEVICE_PANEL].power_W;
	
	        v_salida = g_ina219_data[INA219_DEVICE_BATTERY].bus_voltage_V;
	        i_salida = g_ina219_data[INA219_DEVICE_BATTERY].current_A;
	        w_salida = g_ina219_data[INA219_DEVICE_BATTERY].power_W;
			
			for(int k=0; k<LDR_COUNT; k++) ldr_r[k] = g_ldr_data[k].resistance_kohm;

            xSemaphoreGive(g_data_mutex);
		} else {
            ESP_LOGW(TAG, "No se pudo obtener Mutex para leer datos");
        }
    	

		// OJO: aquí asumimos que i_bat > 0 significa que la batería
        // se está DESCARGANDO. Si en tu montaje es al revés,
        // pon: i_bat = -i_bat;
        // para que el SoC suba cuando se cargue y baje cuando se descargue.
		
		soc = battery_soc_update(
            soc,
            v_salida,
            i_salida,
            LOOP_PERIOD_S,
            BAT_CAPACITY_AH
        );
		
		ESP_LOGI(TAG,
                 "IN:  V=%.3f V  I=%.3f A  P=%.3f W | OUT: V=%.3f V  I=%.3f A  P=%.3f W, SoC=%.1f%%",
                 v_entrada, i_entrada, w_entrada, v_salida, i_salida, w_salida, soc);

		ESP_LOGI(TAG, "LDRs: %.2f, %.2f, %.2f, %.2f kOhm", 
                 ldr_r[0], ldr_r[1], ldr_r[2], ldr_r[3]);
	
	
		vTaskDelay(pdMS_TO_TICKS((int)(LOOP_PERIOD_S * 1000)));
    }
}
