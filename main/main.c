#include "battery.h"
#include "ina.h"
#include "adc.h"

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BAT_CAPACITY_AH  2.6f
#define LOOP_PERIOD_S    1.0f


static const char *TAG = "MAIN";

void app_main(void)
{	
	xTaskCreate(ina_task, "ina_task", 4096, NULL, 5, NULL);
    xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);

	vTaskDelay(pdMS_TO_TICKS(1500));

	float soc = 50.0f;
    float v_bat_init = g_ina219_data[INA219_DEVICE_BATTERY].bus_voltage_V;

    if (v_bat_init > 1.0f) {
        soc = battery_porcent_from_voltage(v_bat_init);
        ESP_LOGI(TAG, "SoC inicial estimado: Vbat=%.3f V -> %.1f%%",
                 v_bat_init, soc);
    } else {
        ESP_LOGW(TAG, "No se pudo leer Vbat inicial; usando SoC=50%%");
    }


    while (1) {
    	float v_entrada = g_ina219_data[INA219_DEVICE_PANEL].bus_voltage_V;
        float i_entrada = g_ina219_data[INA219_DEVICE_PANEL].current_A;
        float w_entrada = g_ina219_data[INA219_DEVICE_PANEL].power_W;

        float v_salida  = g_ina219_data[INA219_DEVICE_BATTERY].bus_voltage_V;
        float i_salida  = g_ina219_data[INA219_DEVICE_BATTERY].current_A;
        float w_salida  = g_ina219_data[INA219_DEVICE_BATTERY].power_W;
		

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

		ESP_LOGI(TAG, "LDR1: %.2f kOhm", g_ldr_data[0].resistance_kohm);
		ESP_LOGI(TAG, "LDR1: %.2f kOhm", g_ldr_data[1].resistance_kohm);
		ESP_LOGI(TAG, "LDR1: %.2f kOhm", g_ldr_data[2].resistance_kohm);
		ESP_LOGI(TAG, "LDR1: %.2f kOhm", g_ldr_data[3].resistance_kohm);
	
	
		vTaskDelay(pdMS_TO_TICKS((int)(LOOP_PERIOD_S * 1000)));
    }
}
