#include "esp_log_level.h"
#include "esp_log_timestamp.h"
#include "nvs.h"
#include "esp_err.h"
#include <esp_log.h>
#include "nvs_flash.h"

static const char *TAG = "NVS";
static const char *NS = "storage";
static const char *K_SSID = "SSID";
static const char *K_PASS = "PASS";

void save_wifi_credentials(const char* ssid, const char* pass) {
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open(NS, NVS_READWRITE, &h));
    nvs_set_str(h, K_SSID, ssid);
    nvs_set_str(h, K_PASS, pass);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Saved: %s", ssid);
}

bool load_wifi_credentials(char *ssid, size_t ssid_size, char* pass, size_t pass_size) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    
    if (err != ESP_OK) {
		ESP_LOGI(TAG, "NVS vacia o no inicializada");
	}
    
    esp_err_t e1 = nvs_get_str(h, K_SSID, ssid, &ssid_size);
    esp_err_t e2 = nvs_get_str(h, K_PASS, pass, &pass_size);
    nvs_close(h);

    return (e1 == ESP_OK && e2 == ESP_OK);
}

void init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}