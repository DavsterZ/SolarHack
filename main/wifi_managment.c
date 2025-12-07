#include <esp_log.h>
#include <string.h>

#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "web_managment.h"
#include "wifi_managment.h"


#define ESP_MAXIMUM_RETRY  5

static const char *TAG = "WiFi";
static int s_retry_num = 0;

EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
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
		ip_event_got_ip_t* event = (ip_event_got_ip6_t*) event_data;
		ESP_LOGI(TAG, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

uint16_t wifi_scan_networks(wifi_ap_record_t *ap_info, uint16_t max_aps) {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };

    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed");
        return 0;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) return 0;
    if (ap_count > max_aps) ap_count = max_aps;

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_info));
    ESP_LOGI(TAG, "Found %d networks", ap_count);

    return ap_count;
}

void wifi_ap_connect(void) {
    wifi_config_t ap_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .password = ESP_WIFI_AP_PASS,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .max_connection = MAX_CONNECTIONS_AP,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .channel = 1,
        },
    };
    
    if (strlen((char*)ap_config.ap.password) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP mode: %s", ap_config.ap.ssid);
    start_webserver();
}

void wifi_sta_connect(const char* ssid, const char* pass) {
    wifi_config_t wifi_mode_cfg = {
        .sta = {
            .failure_retry_cnt = N_RETRIES_CONNECTIONS,
            .threshold.authmode = AUTHMODE,
        },
    };
    
    strncpy((char*)wifi_mode_cfg.sta.ssid, ssid, sizeof(wifi_mode_cfg.sta.ssid));
    strncpy((char*)wifi_mode_cfg.sta.password, pass, sizeof(wifi_mode_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_mode_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}