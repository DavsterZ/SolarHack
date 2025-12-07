#pragma once

#include <esp_log.h>
#include "nvs.h"

void save_wifi_credentials(const char* ssid, const char* pass);
bool load_wifi_credentials(char *ssid, size_t ssid_size, char* pass, size_t pass_size);
void init_nvs(void);