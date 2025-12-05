#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "adc.h"
//#include "mqtt_protocol.h"
//#include "http_protocol.h"

static const char *TAG = "ADC";

#define ADC_ATTEN   		ADC_ATTEN_DB_12

//#define PROTOCOL			CONFIG_SELECT_PROTOCOL


// ADC_CHANNEL_4 -> GPIO32
// ADC_CHANNEL_5 -> GPIO33
// ADC_CHANNEL_6 -> GPIO34
// ADC_CHANNEL_7 -> GPIO35

static const adc_channel_t s_ldr_channels[LDR_COUNT] = {
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7
};

static const char *s_ldr_names[LDR_COUNT] = {
    "LDR1",
    "LDR2",
    "LDR3",
    "LDR4"
};

ldr_data_t g_ldr_data[LDR_COUNT];

static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
static adc_cali_handle_t s_adc1_cali_handle = NULL;
static bool s_adc_calibrated = false;


static void adc_hw_init(void);
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);
// static void adc_calibration_deinit(adc_cali_handle_t handle);
static int calc_ohm(int raw_data);

//#define PROTOCOL			CONFIG_SELECT_PROTOCOL

void adc_task(void *pvParameters) {
    (void) pvParameters;
    
    adc_hw_init();
   	while (1) {
		for (int i = 0; i < LDR_COUNT; i++) {
			int adc_raw = 0;
			int adc_voltage = 0;
			
			ESP_ERROR_CHECK(adc_oneshot_read(s_adc1_handle, s_ldr_channels[i], &adc_raw));
		
			if(s_adc_calibrated)
				ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_adc1_cali_handle, adc_raw, &adc_voltage));
			else
				adc_voltage = (int)((adc_raw * 3300) / 4095);
			
			int resistence_ohm = calc_ohm(adc_raw);
			
			// Guardar en el array global
			g_ldr_data[i].raw = adc_raw;
			g_ldr_data[i].voltage_mv = adc_voltage;
			g_ldr_data[i].resistance_kohm = resistence_ohm / 1000.0f;
		
			ESP_LOGI(TAG, "%s -> Raw: %d, Voltage: %d mV, R: %.2f kOhm",
                     s_ldr_names[i],
                     g_ldr_data[i].raw,
                     g_ldr_data[i].voltage_mv,
                     g_ldr_data[i].resistance_kohm);
		}
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}

static void adc_hw_init(void) {
	adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &s_adc1_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    
    for (int i = 0; i < LDR_COUNT; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1_handle, s_ldr_channels[i], &config));
    }

	s_adc_calibrated = adc_calibration_init(ADC_UNIT_1,
                                            ADC_ATTEN,
                                            &s_adc1_cali_handle);
}

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);

    *out_handle = handle;
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration OK");
    } else {
        ESP_LOGW(TAG, "Calibration skipped");
    }

    return (ret == ESP_OK);
}

/*
static void adc_calibration_deinit(adc_cali_handle_t handle) {
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
}
*/

static int calc_ohm(int raw_data) {
    return LDR_MAX_OHM - (LDR_MAX_OHM - LDR_MIN_OHM) * (raw_data / 4095.0);
}