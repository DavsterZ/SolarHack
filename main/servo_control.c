#include "servo_control.h"
#include "esp_log.h"

static const char *TAG = "SERVO";
static bool timer_configured = false;

static void _configure_timer(void) {
    if (timer_configured) return;

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_MODE,
        .timer_num        = SERVO_TIMER,
        .duty_resolution  = SERVO_TIMER_BIT,
        .freq_hz          = SERVO_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    timer_configured = true;
}

void servo_init(int gpio_pin, ledc_channel_t channel) {
    _configure_timer();

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_MODE,
        .channel        = channel,
        .timer_sel      = SERVO_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = gpio_pin,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void servo_set_angle(ledc_channel_t channel, float angle) {
    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_DEGREE) angle = SERVO_MAX_DEGREE;

    uint32_t duty_us = SERVO_MIN_PULSE_US +
                       (uint32_t)((angle / (float)SERVO_MAX_DEGREE) * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US));

    // Convertir us a Duty (Resolución 14 bits -> 16383)
    uint32_t max_duty = (1 << SERVO_TIMER_BIT) - 1;
    uint32_t duty = (duty_us * max_duty) / 20000; // 20000us = 20ms periodo

    ESP_ERROR_CHECK(ledc_set_duty(SERVO_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(SERVO_MODE, channel));
}
