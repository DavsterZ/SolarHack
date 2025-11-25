#include "battery.h"
#include "ina.h"

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"


#define INA_PANEL_ADDR   0x40
#define INA_BAT_ADDR     0x41
#define BAT_CAPACITY_AH  2.6f
#define INA_RSHUNT_OHMS  0.1f	// Cambiar
#define INA_PANEL_IMAX_A 1.0f
#define INA_BAT_IMAX_A   1.0f
#define LOOP_PERIOD_S    1.0f


static const char *TAG = "MAIN";

void app_main(void)
{	
	ina219_t ina_entrada; 	// Hacia Panel
	ina219_t ina_salida;	// Hacia bateria


	// Direccion entrada: 0x40
	// Direcion salida: 0x41
	// Iniciar INA
	ESP_ERROR_CHECK(ina219_init(&ina_entrada, INA_PANEL_ADDR, INA_RSHUNT_OHMS, INA_PANEL_IMAX_A));
	ESP_ERROR_CHECK(ina219_init(&ina_salida, INA_BAT_ADDR, INA_RSHUNT_OHMS, INA_BAT_IMAX_A));

	float soc = 50.0f;
    float v_bat_init = 0.0f;

    if (ina219_read_bus_voltage(&ina_salida, &v_bat_init) == ESP_OK) {
        soc = battery_porcent_from_voltage(v_bat_init);
        ESP_LOGI(TAG, "SoC inicial estimado: Vbat=%.3f V -> %.1f%%",
                 v_bat_init, soc);
    } else {
        ESP_LOGW(TAG, "No se pudo leer Vbat inicial; usando SoC=50%%");
    }


    while (true) {
        float v_entrada = 0.0f, i_entrada = 0.0f, w_entrada = 0.0f;
		float v_salida = 0.0f, i_salida = 0.0f, w_salida = 0.0f;

		
		if (ina219_read_bus_voltage(&ina_entrada ,&v_entrada) == ESP_OK && 
			ina219_read_bus_voltage(&ina_salida ,&v_salida) == ESP_OK && 
			ina219_read_bus_voltage(&ina_salida ,&i_entrada) == ESP_OK && 
			ina219_read_bus_voltage(&ina_salida ,&i_salida) == ESP_OK && 
			ina219_read_bus_voltage(&ina_salida ,&w_entrada) == ESP_OK && 
			ina219_read_bus_voltage(&ina_salida ,&w_salida) == ESP_OK)
		{	

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
		}
		else
			ESP_LOGE(TAG, "Error leyendo algunos de los INA219");

		vTaskDelay(pdMS_TO_TICKS((int)(LOOP_PERIOD_S * 1000)));
    }
}
