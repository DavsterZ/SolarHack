#pragma once

#include <stdint.h>

#define LDR_MIN_OHM         CONFIG_LDR_MIN_OHM
#define LDR_MAX_OHM         CONFIG_LDR_MAX_OHM

#define LDR_COUNT 			4

// Estructura con los datso de cada LDR
typedef struct {
	int32_t raw;
	int32_t voltage_mv;
	float resistance_kohm;
} ldr_data_t;

// Array global con las lecturas de los 4 LDRs
extern ldr_data_t g_ldr_data[LDR_COUNT];

void adc_task(void *pvParameters);