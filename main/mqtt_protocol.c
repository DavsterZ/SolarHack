#include <stdio.h> 
#include <stdint.h> 
#include <stddef.h> 
#include <string.h>
#include "esp_err.h"

#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h" 
#include "cJSON.h"
#include "mqtt_protocol.h"


#define BROKER_URL_MQTT CONFIG_BROKER_URL_MQTT
#define ACCESS_TOKEN "4NBca7arMs11qx2F7P65"

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t client = NULL;
static bool s_mqtt_connected = false;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){ 
	ESP_LOGD(TAG, "Event dispatch base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Conectado");
        s_mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Desconectado");
        s_mqtt_connected = false;
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT Error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
        }
        break;
    default:
        break;
    }
}


void mqtt_app_start(void)
{
    if (client != NULL) return; // Ya iniciado

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL_MQTT,
        .credentials.username = CONFIG_MQTT_TOKEN, // Token para ThingsBoard
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

int mqtt_send_telemetry(ina219_data_t *panel, ina219_data_t *bat, float soc, ldr_data_t *ldrs)
{
    if (!client || !s_mqtt_connected) {
        ESP_LOGW(TAG, "No se puede publicar: Cliente no conectado");
        return -1;
    }

    cJSON *root = cJSON_CreateObject();
    
    // Datos Panel Solar
    cJSON_AddNumberToObject(root, "panel_v", panel->bus_voltage_V);
    cJSON_AddNumberToObject(root, "panel_i", panel->current_A);
    cJSON_AddNumberToObject(root, "panel_p", panel->power_W);

    // Datos Batería
    cJSON_AddNumberToObject(root, "bat_v", bat->bus_voltage_V);
    cJSON_AddNumberToObject(root, "bat_i", bat->current_A);
    cJSON_AddNumberToObject(root, "bat_p", bat->power_W);
    cJSON_AddNumberToObject(root, "bat_soc", soc);

    // Datos LDRs (enviamos resistencia kOhm)
    for(int i = 0; i < LDR_COUNT; i++) {
        char label[10];
        // Crea etiquetas: "ldr_1", "ldr_2", etc.
        snprintf(label, sizeof(label), "ldr_%d", i + 1);
        
        // Añade directamente al objeto root
        cJSON_AddNumberToObject(root, label, ldrs[i].resistance_kohm);
    }

    char *post_data = cJSON_PrintUnformatted(root);
    
    // Publicar al tópico definido en Kconfig
    int msg_id = esp_mqtt_client_publish(client, CONFIG_MQTT_TOPIC_TELEMETRY, post_data, 0, 1, 0);
    
    cJSON_Delete(root);
    free(post_data);

    if(msg_id >= 0) {
        ESP_LOGI(TAG, "Telemetría enviada OK, msg_id=%d", msg_id);
    } else {
        ESP_LOGE(TAG, "Error enviando telemetría");
    }

    return msg_id;
}
