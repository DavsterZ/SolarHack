#include "solar_tracker.h"
#include "adc.h"
#include "servo_control.h"
#include "protect.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h> 
#include <math.h>


static const char *TAG = "TRACKER";

#define PIN_SERVO_H       CONFIG_SERVO_PIN_HORIZ
#define PIN_SERVO_V       CONFIG_SERVO_PIN_VERT
#define TOLERANCE         CONFIG_TRACKER_TOLERANCE
#define CYCLE_MS          CONFIG_TRACKER_UPDATE_MS
#define STEP_DEG          strtof(CONFIG_TRACKER_STEP_DEG, NULL)
#define CHANNEL_H         LEDC_CHANNEL_0
#define CHANNEL_V         LEDC_CHANNEL_1

// Asignación de Índices LDR
#define IDX_TOP    0 
#define IDX_BOT    1 
#define IDX_LEFT   2 
#define IDX_RIGHT  3

tracker_data_t g_tracker_data = { .angle_h = 90.0f, .angle_v = 45.0f };

// Estado actual de los ángulos
static float s_angle_h = 90.0f; // Empezar centrado
static float s_angle_v = 45.0f; // Empezar a 45 grados

static void tracker_task(void *pvParameters)
{
	ESP_LOGI(TAG, "Tarea Tracker iniciada");

	// Pequeña espera para asegurar que ADC ya tiene datos
    vTaskDelay(pdMS_TO_TICKS(2000));
	
	while (1) {
        int raw[LDR_COUNT] = {0};
        bool data_ok = false;

        // Leer datos globales de forma segura
        if (g_data_mutex != NULL) {
            if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                for (int i = 0; i < LDR_COUNT; i++) {
                    raw[i] = g_ldr_data[i].raw;
                }
                xSemaphoreGive(g_data_mutex);
                data_ok = true;
            }
        }

        if (data_ok) {
            // Calcular promedios/diferencias
            int val_top   = raw[IDX_TOP];
            int val_bot   = raw[IDX_BOT];
            int val_left  = raw[IDX_LEFT];
            int val_right = raw[IDX_RIGHT];

            // Lógica Vertical
            int diff_v = val_top - val_bot;
            if (abs(diff_v) > TOLERANCE) {
                // Si Top > Bot -> Sol arriba (asumiendo Raw alto = más luz)
                // Ajustar signo (+/-) según la mecánica de tu servo
                if (val_top > val_bot) s_angle_v -= STEP_DEG; 
                else                   s_angle_v += STEP_DEG;
            }

            // Lógica Horizontal
            int diff_h = val_left - val_right;
            if (abs(diff_h) > TOLERANCE) {
                 // Si Left > Right -> Sol a la izquierda
                if (val_left > val_right) s_angle_h += STEP_DEG;
                else                      s_angle_h -= STEP_DEG;
            }

            // Límites de seguridad (0 a 180 grados)
            if (s_angle_v < 0.0f) s_angle_v = 0.0f;
            if (s_angle_v > 180.0f) s_angle_v = 180.0f;

            if (s_angle_h < 0.0f) s_angle_h = 0.0f;
            if (s_angle_h > 180.0f) s_angle_h = 180.0f;

            // Mover Servos
            servo_set_angle(CHANNEL_V, s_angle_v);
            servo_set_angle(CHANNEL_H, s_angle_h);

			if (g_data_mutex != NULL) {
                if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    g_tracker_data.angle_h = s_angle_h;
                    g_tracker_data.angle_v = s_angle_v;
                    xSemaphoreGive(g_data_mutex);
                }
            }

            // Log opcional para depuración (nivel VERBOSE para no saturar)
            ESP_LOGV(TAG, "V:%.1f H:%.1f | T:%d B:%d L:%d R:%d", 
                     s_angle_v, s_angle_h, val_top, val_bot, val_left, val_right);
        }

        vTaskDelay(pdMS_TO_TICKS(CYCLE_MS));
    }
}

void solar_tracker_start(void) {
    // Inicializar Hardware de Servos
    servo_init(PIN_SERVO_H, CHANNEL_H);
    servo_init(PIN_SERVO_V, CHANNEL_V);

    // Mover a posición inicial
    servo_set_angle(CHANNEL_H, s_angle_h);
    servo_set_angle(CHANNEL_V, s_angle_v);

    // Crear la tarea
    xTaskCreate(tracker_task, "tracker_logic", 4096, NULL, 5, NULL);
}

void solar_tracker_park(void)
{
	ESP_LOGI(TAG, "Aparcando servos para dormir...");

    // Ajusta estos valores según tu montaje físico
    float park_h = 90.0f; 
    float park_v = 0.0f;  

    servo_set_angle(CHANNEL_H, park_h);
    servo_set_angle(CHANNEL_V, park_v);
    
    // Actualizamos la estructura global por si se envía un último MQTT
    if (g_data_mutex != NULL) {
        if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_tracker_data.angle_h = park_h;
            g_tracker_data.angle_v = park_v;
            xSemaphoreGive(g_data_mutex);
        }
    }

    // Importante: Dar tiempo físico a los motores para llegar a la posición
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Servos aparcados.");
}