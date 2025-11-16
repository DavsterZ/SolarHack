#include "ina.h"

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

void app_main(void)
{	
	// Iniciar INA
	esp_err_t err = ina219_init();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "INA219 init fallo");
		return;
	}

    while (true) {
        float bus_v = 0.0f;
		err = ina219_read_bus_voltage(&bus_v);
		if (err == ESP_OK)
			ESP_LOGI(TAG, "Bus voltage = %.3f V", bus_v);
		else
			ESP_LOGE(TAG, "Error leyendo bus voltage: %s", esp_err_to_name(err));

		vTaskDelay(pdMS_TO_TICKS(500));
    }
}
