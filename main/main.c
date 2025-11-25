#include "ina.h"

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

void app_main(void)
{	
	ina219_t ina_entrada; 	// Hacia Panel
	ina219_t ina_salida;	// Hacia bateria

	// Direccion entrada: 0x40
	// Direcion salida: 0x41
	// Iniciar INA
	ESP_ERROR_CHECK(ina219_init(&ina_entrada, 0x40));
	ESP_ERROR_CHECK(ina219_init(&ina_salida, 0x41));


    while (true) {
        float v_entrada = 0.0f;
		float v_salida = 0.0f;
		
		if (ina219_read_bus_voltage(&ina_entrada ,&v_entrada) == ESP_OK && ina219_read_bus_voltage(&ina_salida ,&v_salida) == ESP_OK)
			ESP_LOGI(TAG, "Entrada = %.3f V, Salida = %.3f V",v_entrada, v_salida);
		else
			ESP_LOGE(TAG, "Error leyendo bus voltage");

		vTaskDelay(pdMS_TO_TICKS(500));
    }
}
