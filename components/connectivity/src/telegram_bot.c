#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "esp_err.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "cJSON.h"

#include "telegram_bot.h"
#include "solar_tracker.h"
#include "ina.h" // Para leer voltajes en el comando /status
#include "protect.h"

static const char *TAG = "TELEGRAM";

#define TELEGRAM_TOKEN 		CONFIG_TELEGRAM_TOKEN
#define TELEGRAM_CHAT_ID 	CONFIG_TELEGRAM_CHAT_ID
#define POLLING_INTERVAL_MS	CONFIG_POLLING_INTERVAL_MS

// Control para mismos mensajes
static int64_t last_update_id = 0;

void telegram_send_text(const char *format, ...)
{
	char msg_buffer[512];
	va_list args;
	va_start(args, format);
	vsnprintf(msg_buffer, sizeof(msg_buffer), format, args);
	va_end(args);

	ESP_LOGI(TAG, "Enviando respuesta: %s", msg_buffer);

	// Construir URL
	char url[1024];

	esp_http_client_config_t config = {
		.url = "https://api.telegram.org",
		.transport_type = HTTP_TRANSPORT_OVER_SSL,
		.crt_bundle_attach = esp_crt_bundle_attach,
	
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	// Construir URL completa
	snprintf(url,  sizeof(url),  "https://api.telegram.org/bot%s/sendMessage", TELEGRAM_TOKEN);
	esp_http_client_set_url(client, url);
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client, "Content-Type", "application/json");

	// Crear JSON Body
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "chat_id", TELEGRAM_CHAT_ID);
	cJSON_AddStringToObject(root, "text", msg_buffer);
	char *post_data = cJSON_PrintUnformatted(root);

	esp_http_client_set_post_field(client, post_data, strlen(post_data));

	esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Mensaje enviado OK");
    } else {
        ESP_LOGE(TAG, "Error enviando mensaje: %s", esp_err_to_name(err));
    }

    cJSON_Delete(root);
    free(post_data);
    esp_http_client_cleanup(client);
}


static void handle_command(char *text)
{
	ESP_LOGI(TAG, "Comando recibido: %s", text);

	if (strncmp(text, "/start", 6) == 0 || strncmp(text, "/help", 5) == 0)
	{
		telegram_send_text("Comandos Disponibles:\n"
                           "/status - Voltaje y Bateria\n"
                           "/park - Aparcar servos (Seguro)\n"
                           "/sleep - Forzar Deep Sleep\n"
                           "/reset - Reiniciar ESP32");
	}
	else if (strncmp(text, "/status", 7) == 0)
	{
		float v_bat = 0, soc = 0, v_panel = 0;
		if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(200))) {
            v_bat = g_ina219_data[INA219_DEVICE_BATTERY].bus_voltage_V;
            v_panel = g_ina219_data[INA219_DEVICE_PANEL].bus_voltage_V;
            soc = g_battery_soc;
            xSemaphoreGive(g_data_mutex);
        }
		telegram_send_text("🔋 Estado:\nBateria: %.2f V (%.1f%%)\nPanel: %.2f V", v_bat, soc, v_panel);
	}
	else if (strncmp(text, "/park", 5) == 0) {
        telegram_send_text("🚧 Aparcando servos...");
        solar_tracker_park();
        telegram_send_text("✅ Servos aparcados.");
    }
	else if (strncmp(text, "/sleep", 6) == 0) {
        telegram_send_text("💤 Entrando en Deep Sleep forzado (1 min)...");
        
        ESP_LOGI(TAG, "Confirmando mensaje a Telegram antes de dormir...");
        
        esp_http_client_config_t config = {
            .url = "https://api.telegram.org",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 2000, 
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        
        // Construimos la URL para confirmar el mensaje actual (offset + 1)
        char url[512];
        snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates?offset=%lld&limit=1&timeout=0", TELEGRAM_TOKEN, last_update_id + 1);
        
        esp_http_client_set_url(client, url);
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        
        // Enviamos la confirmación y esperamos a que termine
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Mensaje confirmado (Flush OK).");
        } else {
            ESP_LOGW(TAG, "Fallo al confirmar mensaje: %s", esp_err_to_name(err));
        }
        
        esp_http_client_cleanup(client);

        // Pequeña espera para asegurar que la transmisión se complete físicamente
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        
        // Configurar tiempo (ejemplo: 1 minuto = 60 seg)
        esp_sleep_enable_timer_wakeup(60 * 1000000ULL); 
        esp_deep_sleep_start();
    }
	else if (strncmp(text, "/reset", 6) == 0) {
        telegram_send_text("🔄 Reiniciando...");
        
        ESP_LOGI(TAG, "Confirmando mensaje a Telegram antes del reset...");
        
        esp_http_client_config_t config = {
            .url = "https://api.telegram.org",
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 2000,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        
        // Hacemos una petición 'getUpdates' con offset = last_update_id + 1
        // Esto le confirma a Telegram que hemos procesado el mensaje actual.
        char url[512];
        snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates?offset=%lld&limit=1&timeout=0", TELEGRAM_TOKEN, last_update_id + 1);
        
        esp_http_client_set_url(client, url);
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        
        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Mensaje confirmado (Flush OK).");
        } else {
            ESP_LOGW(TAG, "Fallo al confirmar mensaje: %s", esp_err_to_name(err));
        }
        
        esp_http_client_cleanup(client);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Espera de seguridad
        esp_restart();
    }
	else {
        telegram_send_text("❓ Comando no reconocido.");
    }
}


static void check_updates(void)
{
	esp_http_client_config_t config = {
		.url = "https://api.telegram.org",
		.transport_type = HTTP_TRANSPORT_OVER_SSL,
		.crt_bundle_attach = esp_crt_bundle_attach,
		.timeout_ms = POLLING_INTERVAL_MS,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	// URL para getUpdates con offset
	char url[512];
	snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates?offset=%lld&limit=1&timeout=0", TELEGRAM_TOKEN, last_update_id + 1);

	esp_http_client_set_url(client, url);
	esp_http_client_set_method(client, HTTP_METHOD_GET);
	
	// Ejecutar peticion
	esp_err_t err = esp_http_client_open(client, 0);
	if (err == ESP_OK)
	{
		int content_length = esp_http_client_fetch_headers(client);
		if (content_length > 0)	
		{
			// Leer respuesta
			char *buffer = malloc(content_length + 1);
			if (buffer)
			{
				int read_len = esp_http_client_read_response(client, buffer, content_length);
				buffer[read_len] = 0;

				// Parsear JSON
				cJSON * json = cJSON_Parse(buffer);
				if (json)
				{
					cJSON *ok = cJSON_GetObjectItem(json, "ok");
					cJSON *result = cJSON_GetObjectItem(json, "result");

					if (cJSON_IsTrue(ok) && cJSON_IsArray(result))
					{
						int num_msgs = cJSON_GetArraySize(result);
						for (int i = 0; i < num_msgs; i++)
						{
							cJSON *item = cJSON_GetArrayItem(result, i);
							cJSON *update_id = cJSON_GetObjectItem(item, "update_id");

							// Guardar ultimo ID para no repetir
							if (update_id) last_update_id = update_id->valuedouble;

							cJSON *message = cJSON_GetObjectItem(item, "message");
							if (message)
							{
								cJSON *text = cJSON_GetObjectItem(message, "text");
								cJSON *chat = cJSON_GetObjectItem(message, "chat");
								cJSON *chat_id = cJSON_GetObjectItem(chat, "id");

								// Verificar seguridad: Solo responder a mi chat id
								char id_str[32];
								snprintf(id_str, sizeof(id_str), "%.0f", chat_id->valuedouble);

								if (strcmp(id_str, TELEGRAM_CHAT_ID) == 0 && text)
									handle_command(text->valuestring);
								else
									ESP_LOGW(TAG, "Intento de acceso no autorizado ID: %s", id_str);
								
							}
						}
					}
					cJSON_Delete(json);
				}
				free(buffer);
			}
		}
	} else {
		ESP_LOGE(TAG, "Fallo HTTP GET: %s", esp_err_to_name(err));
	}

	esp_http_client_cleanup(client);
}


static void telegram_task(void *pvParameters)
{
	ESP_LOGI(TAG, "Bot iniciado. Escuchando comandos...");
    telegram_send_text("🔌 Sistema Solar Online. Escribe /help para ver comandos.");

    while (1) {
        check_updates();
        vTaskDelay(pdMS_TO_TICKS(POLLING_INTERVAL_MS));
    }
}

void telegram_bot_start(void)
{
    xTaskCreate(telegram_task, "telegram_task", 6144, NULL, 5, NULL);
}

