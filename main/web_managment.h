#pragma once

#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include <esp_log.h>

esp_err_t wifi_post_handler(httpd_req_t *req);

esp_err_t wifi_get_handler(httpd_req_t *req);

httpd_handle_t start_webserver(void);

void url_decode(char *src, char *dest);