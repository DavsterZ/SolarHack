#include "esp_http_server.h"
#include "esp_err.h"
#include <esp_log.h>
#include <stdlib.h>

#include "web_managment.h"
#include "nvs_managment.h"
#include "wifi_managment.h"

static const char *TAG_WEB = "WEB";

void url_decode(char *src, char *dest) {
    char *psrc = src, *pdest = dest;
    while (*psrc) {
        if (*psrc == '%') {
            int val;
            sscanf(psrc + 1, "%2x", &val);
            *pdest = (char)val;
            psrc += 3;
        } else {
            *pdest = (*psrc == '+') ? ' ' : *psrc;
            psrc++;
        }
        pdest++;
    }
    *pdest = '\0';
}

esp_err_t wifi_get_handler(httpd_req_t *req) {
    wifi_ap_record_t ap_info[MAX_APS];
    memset(ap_info, 0, sizeof(ap_info));
    
    uint16_t ap_count = wifi_scan_networks(ap_info, MAX_APS);
    httpd_resp_set_type(req, "text/html");

    const char *h1 = 
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>WiFi</title><style>"
        "body{font-family:Arial;margin:20px;background:#f0f0f0}"
        ".c{max-width:500px;margin:0 auto;background:#fff;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
        "h2{text-align:center;color:#333}"
        ".n{padding:10px;margin:8px 0;border:1px solid #ddd;border-radius:5px;cursor:pointer;background:#f9f9f9}"
        ".n:hover{background:#e0e0e0}"
        ".n.s{background:#d0e8ff;border-color:#06c}"
        ".sg{color:#06c;font-size:12px}"
        "input[type='password']{width:100%;padding:10px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box}"
        "input[type='submit']{width:100%;padding:12px;background:#06c;color:#fff;border:none;border-radius:5px;cursor:pointer;font-size:16px}"
        "input[type='submit']:hover{background:#05a}"
        "#p{display:none;margin-top:15px}"
        "</style></head><body><div class='c'><h2>WiFi Config</h2>";

    httpd_resp_sendstr_chunk(req, h1);

    if (ap_count == 0) {
        httpd_resp_sendstr_chunk(req, "<p style='text-align:center;color:#999'>No networks</p>");
    } else {
        httpd_resp_sendstr_chunk(req, "<p style='text-align:center;color:#666'>Select network:</p>");

        char buf[256];
        for (int i = 0; i < ap_count; i++) {
            int np = (ap_info[i].authmode != WIFI_AUTH_OPEN);
            snprintf(buf, sizeof(buf),
                "<div class='n' onclick='sel(\"%s\",%d)'>"
                "<strong>%s</strong><br>"
                "<span class='sg'>%d dBm | %s</span></div>",
                (char*)ap_info[i].ssid, np, (char*)ap_info[i].ssid,
                ap_info[i].rssi, np ? "Locked" : "Open");
            httpd_resp_sendstr_chunk(req, buf);
        }
    }

    const char *h2 =
        "<div id='p'><form method='POST' action='/setwifi'>"
        "<input type='hidden' id='ssid' name='ssid' value=''>"
        "<label><strong>Password:</strong></label>"
        "<input type='password' id='pass' name='pass' placeholder='Enter password' autocomplete='off'>"
        "<input type='submit' value='Save'></form></div></div>"
        "<script>"
        "function sel(s,p){"
        "document.querySelectorAll('.n').forEach(el=>el.classList.remove('s'));"
        "event.currentTarget.classList.add('s');"
        "document.getElementById('ssid').value=s;"
        "document.getElementById('p').style.display='block';"
        "document.getElementById('pass').required=(p==1);"
        "if(p==0)document.getElementById('pass').value='';"
        "}</script></body></html>";

    httpd_resp_sendstr_chunk(req, h2);
    httpd_resp_sendstr_chunk(req, NULL);
    
    return ESP_OK;
}

esp_err_t wifi_post_handler(httpd_req_t *req) {
    char ssid[33] = {0}, pass[65] = {0};
    size_t sz = req->content_len;
    
    if (sz > 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Too large");
        return ESP_FAIL;
    }

    char *buf = malloc(sz + 1);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    if (httpd_req_recv(req, buf, sz) <= 0) {
        free(buf);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[sz] = '\0';
    
    char val_ssid[100] = {0}, val_pass[100] = {0};

    if (httpd_query_key_value(buf, "ssid", val_ssid, sizeof(val_ssid)) != ESP_OK ||
    	httpd_query_key_value(buf, "pass", val_pass, sizeof(val_pass)) != ESP_OK) 
    {
			
	}

    url_decode(ssid, ssid);
    url_decode(pass, pass);
    free(buf);

    if (strlen(ssid) == 0) {
        httpd_resp_sendstr(req, "<html><body><h2>Error</h2><p>Empty SSID</p></body></html>");
        return ESP_FAIL;
    }

    save_wifi_credentials(ssid, pass);

    httpd_resp_sendstr(req, 
        "<html><head><meta charset='UTF-8'></head><body style='font-family:Arial;text-align:center;margin-top:50px'>"
        "<h2>Saved</h2><p>Restarting...</p></body></html>");
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    
    httpd_handle_t server = NULL;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t get_uri = {.uri = "/", .method = HTTP_GET, .handler = wifi_get_handler, .user_ctx = NULL};
        httpd_uri_t post_uri = {.uri = "/setwifi", .method = HTTP_POST, .handler = wifi_post_handler, .user_ctx = NULL};
        httpd_register_uri_handler(server, &get_uri);
        httpd_register_uri_handler(server, &post_uri);
        ESP_LOGI(TAG_WEB, "Server started");
    } else {
        ESP_LOGE(TAG_WEB, "Server start failed");
    }
    return server;
}