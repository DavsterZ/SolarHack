#pragma once

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include <stdint.h>

// Configuración por defecto del AP del ESP32
#define ESP_WIFI_AP_SSID        "ESP32_CONFIG"
#define ESP_WIFI_AP_PASS        ""
#define MAX_CONNECTIONS_AP      4
#define MAX_APS                 20


#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

extern EventGroupHandle_t s_wifi_event_group;

void wifi_init_system(void);
void wifi_start_ap(void);
void wifi_start_sta(const char* ssid, const char* pass);
uint16_t wifi_scan_networks(wifi_ap_record_t *ap_info, uint16_t max_aps);