#pragma once

#include <stddef.h> 
#include "adc.h"
#include "ina.h"
#include "solar_tracker.h"


void mqtt_app_start(void);

int mqtt_send_telemetry(ina219_data_t *panel, ina219_data_t *bat, float soc, ldr_data_t *ldrs, tracker_data_t *tracker);