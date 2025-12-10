#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Mutex global para proteger la lectura/escritura e datos compartidos
extern SemaphoreHandle_t g_data_mutex;