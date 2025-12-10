// Une los datos leidos por el ADC con el servo
#pragma once

typedef struct {
    float angle_h; // Ángulo Horizontal (Azimut)
    float angle_v; // Ángulo Vertical (Elevación)
} tracker_data_t;

// Variable global para que el Main la lea
extern tracker_data_t g_tracker_data;

// Iniciar el hardware de servos y la tarea de seguimiento
void solar_tracker_start(void);

// Mueve los servos a posición de reposo (ej: 90 grados H, 0 grados V)
void solar_tracker_park(void);