#pragma once

// Calcula un % aproximado de la bateria a partir del voltaje en reposo
float battery_porcent_from_voltage(float volts);

float battery_soc_update(float soc_prev, float v_bat, float current_A, float dt_s, float capacity_Ah);