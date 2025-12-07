#include <esp_log.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "web_managment.h"
#include "wifi_managment.h"


#define ESP_MAXIMUM_RETRY  5

static const char *TAG = "WiFi";
static int s_retry_num = 0;

EventGroupHandle_t s_wifi_event_group;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) 
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();	
   	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGD(TAG, "Reintentando conectar al AP...");
		}
		else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			ESP_LOGE(TAG, "Fallo al conectar tras varios intentos.");
		}
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_init_system(void)
{
	s_wifi_event_group = xEventGroupCreate();
	
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,	
														ESP_EVENT_ANY_ID, 
														&event_handler, 
														NULL,
														NULL));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,	
														IP_EVENT_STA_GOT_IP, 
														&event_handler, 
														NULL,
														NULL));
}

void wifi_start_ap(void) 
{
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = ESP_WIFI_AP_SSID,
			.ssid_len = strlen(ESP_WIFI_AP_SSID),
			.channel = 1,
			.password = ESP_WIFI_AP_PASS,
			.max_connection = MAX_CONNECTIONS_AP,
			.authmode = WIFI_AUTH_WPA2_PSK
		},
	};

	if (strlen(ESP_WIFI_AP_SSID) == 0)
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // APSTA para poder escanear
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "Modo AP iniciado. SSID: %s", ESP_WIFI_AP_SSID);
    
    // Iniciar servidor web aquí
    start_webserver();
}

void wifi_start_sta(const char *ssid, const char *pass)
{
	wifi_config_t wifi_config = {
		.sta = {
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
		},
	};

	strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Iniciando modo STA. Intentando conectar a %s...", ssid);
}

uint16_t wifi_scan_networks(wifi_ap_record_t *ap_info, uint16_t max_aps) {
    wifi_scan_config_t scan_config = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) return 0;

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count > max_aps) ap_count = max_aps;
    
    if (ap_count > 0) {
        esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    }
    return ap_count;
}
