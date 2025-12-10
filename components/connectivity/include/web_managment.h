#pragma once

#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include <esp_log.h>


void register_ota_handlers(httpd_handle_t server);

httpd_handle_t start_webserver(void);

void url_decode(char *src, char *dest);