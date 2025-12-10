#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "driver/i2c.h"

#include "ina.h"
#include "adc.h"
#include "protect.h"
#include "battery.h"
#include "nvs_managment.h"
#include "wifi_managment.h"
#include "mqtt_protocol.h"
#include "solar_tracker.h"

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define BAT_CAPACITY_AH  strtof(CONFIG_BAT_CAPACITY_AH, NULL)
#define LOOP_PERIOD_S    ((float)CONFIG_MAIN_LOOP_PERIOD_S)

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

	init_nvs();

	wifi_init_system();
	
	char ssid[33] = {0};
    char pass[65] = {0};
    bool has_creds = load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass));

	if (!has_creds) 
	{
        // --- MODO CONFIGURACIÓN (AP) ---
        ESP_LOGI(TAG, "No hay credenciales. Iniciando Modo AP...");
        wifi_start_ap();
        
        // El bucle while(1) solo sirve para mantener el main vivo si fuera necesario, 
        // pero el webserver funciona por callbacks.
        while(1) { 
			vTaskDelay(pdMS_TO_TICKS(1000)); 
		}
    } else 
	{
		ESP_LOGI(TAG, "Credenciales encontradas. Conectando a %s...", ssid);
		
		i2c_master_init();

		wifi_start_sta(ssid, pass);

		// Esperar a tener IP antes de lanzar tareas
        // Esperamos bits CONNECTED o FAIL
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);

		if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Conectado. Iniciando Tareas de Sensores...");
            
            httpd_handle_t server = start_webserver();

            if (server != NULL) {
                register_ota_handlers(server);
            }

            // Aquí lanzamos tus tareas
            xTaskCreate(ina_task, "ina_task", 4096, NULL, 5, NULL);
            xTaskCreate(adc_task, "adc_task", 4096, NULL, 5, NULL);
			
			mqtt_app_start();			
				
			solar_tracker_start();			

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
            
            // Loop principal (Monitorización, MQTT, etc)
            while(1) {
                ina219_data_t d_panel = {0};
                ina219_data_t d_bat = {0};
                ldr_data_t d_ldrs[LDR_COUNT]; // Array local
				tracker_data_t d_tracker = {0};
                bool data_ok = false;
		
				if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
					d_panel = g_ina219_data[INA219_DEVICE_PANEL];
                    d_bat   = g_ina219_data[INA219_DEVICE_BATTERY];
                    
                    // Copia eficiente del array de LDRs
                    memcpy(d_ldrs, g_ldr_data, sizeof(ldr_data_t) * LDR_COUNT);

					d_tracker = g_tracker_data;
                    
                    data_ok = true;
		            xSemaphoreGive(g_data_mutex);
				} else {
		            ESP_LOGW(TAG, "No se pudo obtener Mutex para leer datos");
		        }

				if (data_ok) {
                    // Actualización del SoC (Coulomb Counting + Voltaje)
                    soc = battery_soc_update(
                        soc,
                        d_bat.bus_voltage_V,
                        d_bat.current_A,
                        LOOP_PERIOD_S,
                        BAT_CAPACITY_AH
                    );
                    
                    // Loguear en consola
                    ESP_LOGI(TAG, "Panel: %.2fW | Bat: %.2f%% | Voltage: %.2fV| Servos H:%.1f V:%.1f",
                             d_panel.power_W, soc, d_panel.bus_voltage_V, d_tracker.angle_h, d_tracker.angle_v);
            
                    // Enviar Telemetría MQTT
                    // Pasamos las direcciones de las estructuras locales
                    mqtt_send_telemetry(&d_panel, &d_bat, soc, d_ldrs, &d_tracker);
                }

		    	vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_S * 1000));
            }

        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGE(TAG, "Fallo al conectar WiFi. ¿Credenciales mal? ¿Resetear NVS?");
            // Aquí podrías decidir borrar NVS y reiniciar tras X fallos
            // O arrancar las tareas en modo 'Offline'
        }
	}
}
