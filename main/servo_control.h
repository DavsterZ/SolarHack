#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "driver/ledc.h"

// Configuración PWM Servo
#define SERVO_MODE             LEDC_LOW_SPEED_MODE // Compatible con S3/C3/Original
#define SERVO_TIMER            LEDC_TIMER_0
#define SERVO_TIMER_BIT        LEDC_TIMER_14_BIT
#define SERVO_FREQ_HZ          50

#define SERVO_MIN_PULSE_US     500
#define SERVO_MAX_PULSE_US     2500
#define SERVO_MAX_DEGREE       180

// Inicializa un servo en un pin y canal específico
void servo_init(int gpio_pin, ledc_channel_t channel);

// Mueve el servo
void servo_set_angle(ledc_channel_t channel, float angle);

#endif
