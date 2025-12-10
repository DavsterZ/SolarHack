#include "esp_http_server.h"
#include "esp_err.h"
#include <esp_log.h>
#include <stdlib.h>
#include <sys/param.h>

#include "esp_ota_ops.h"
#include "esp_partition.h"

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

static esp_err_t wifi_get_handler(httpd_req_t *req) {
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


static esp_err_t wifi_post_handler(httpd_req_t *req) {
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
		ESP_LOGW(TAG_WEB, "Faltan parámetros ssid o pass en el POST");
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID or Password");
        return ESP_FAIL;
	}

    url_decode(val_ssid, ssid);
    url_decode(val_pass, pass);
    
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


static esp_err_t ota_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    const char *html = 
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>OTA Update</title>"
        "<style>"
        "body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center}"
        ".c{max-width:400px;margin:0 auto;background:#fff;padding:30px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
        "input[type='file']{margin-bottom:20px}"
        "button{padding:10px 20px;background:#06c;color:white;border:none;border-radius:5px;cursor:pointer}"
        "#bar{width:100%;background:#ddd;height:20px;margin-top:20px;display:none}"
        "#prog{width:0%;height:100%;background:#4caf50}"
        "</style></head><body>"
        "<div class='c'><h2>System Update</h2>"
        "<input type='file' id='f'><br>"
        "<button onclick='up()'>Upload & Update</button>"
        "<div id='bar'><div id='prog'></div></div>"
        "<p id='st'></p></div>"
        "<script>"
        "function up(){"
        "var f=document.getElementById('f').files[0];"
        "if(!f){alert('Select file!');return;}"
        "var xhr=new XMLHttpRequest();"
        "xhr.open('POST','/ota',true);"
        "xhr.upload.onprogress=function(e){"
        "if(e.lengthComputable){"
        "var p=(e.loaded/e.total)*100;"
        "document.getElementById('bar').style.display='block';"
        "document.getElementById('prog').style.width=p+'%';"
        "document.getElementById('st').innerHTML='Uploading: '+Math.floor(p)+'%';"
        "}"
        "};"
        "xhr.onload=function(){"
        "if(xhr.status==200) document.getElementById('st').innerHTML='Success! Rebooting...';"
        "else document.getElementById('st').innerHTML='Error: '+xhr.status;"
        "};"
        "xhr.send(f);"
        "}"
        "</script></body></html>";
    
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


static esp_err_t ota_post_handler(httpd_req_t *req) {
    char buf[1024];
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    esp_err_t err;

    ESP_LOGI(TAG_WEB, "Iniciando OTA...");

    // 1. Buscar la siguiente partición OTA libre
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG_WEB, "No hay partición OTA disponible");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 2. Iniciar la escritura OTA
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_WEB, "Error en esp_ota_begin");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 3. Bucle de lectura del socket y escritura en flash
    int received;
    int remaining = req->content_len;

    while (remaining > 0) {
        // Leer datos del HTTP
        if ((received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // Reintentar si es timeout
            }
            ESP_LOGE(TAG_WEB, "Error recibiendo datos");
            esp_ota_end(update_handle);
            return ESP_FAIL;
        }

        // Escribir datos en la partición OTA
        err = esp_ota_write(update_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG_WEB, "Error escribiendo en flash");
            esp_ota_end(update_handle);
            return ESP_FAIL;
        }
        remaining -= received;
    }

    // 4. Finalizar OTA
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_WEB, "Error al finalizar OTA: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 5. Configurar boot partition para arrancar la nueva versión
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_WEB, "Error al setear boot partition");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_WEB, "OTA Completada con éxito. Reiniciando en 2s...");
    httpd_resp_sendstr(req, "OK");

    // Reiniciar para aplicar cambios
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}


httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 8; // Asegurar espacio para handlers
    
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

void register_ota_handlers(httpd_handle_t server) 
{
	if (server == NULL) return;

    httpd_uri_t get_ota = {
        .uri = "/ota",
        .method = HTTP_GET,
        .handler = ota_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &get_ota);

    httpd_uri_t post_ota = {
        .uri = "/ota",
        .method = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &post_ota);

    ESP_LOGI(TAG_WEB, "Endpoints OTA registrados correctamente");
}