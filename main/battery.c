#include "battery.h"

#include <math.h>

// Tabla de búsqueda para la curva de descarga (ajustar según tu batería LiPo/Li-Ion real)
typedef struct {
    float voltage;
    float percent;
} bat_lut_t;

static const bat_lut_t c_bat_lut[] = {
    {4.20f, 100.0f},
    {4.10f, 90.0f},
    {4.00f, 80.0f},
    {3.90f, 70.0f},
    {3.80f, 60.0f},
    {3.75f, 50.0f},
    {3.70f, 40.0f},
    {3.65f, 30.0f},
    {3.55f, 20.0f},
    {3.45f, 10.0f},
    {3.30f, 0.0f}   // Punto mínimo
};

float battery_porcent_from_voltage(float volts)
{
	// Límites superiores e inferiores
    if (volts >= c_bat_lut[0].voltage) return 100.0f;
    
    int len = sizeof(c_bat_lut) / sizeof(c_bat_lut[0]);
    if (volts <= c_bat_lut[len-1].voltage) return 0.0f;

    // Búsqueda e interpolación lineal
    for (int i = 0; i < len - 1; i++) {
        if (volts <= c_bat_lut[i].voltage && volts > c_bat_lut[i+1].voltage) {
            float v_high = c_bat_lut[i].voltage;
            float v_low  = c_bat_lut[i+1].voltage;
            float p_high = c_bat_lut[i].percent;
            float p_low  = c_bat_lut[i+1].percent;

            // Fórmula de la recta entre dos puntos
            return p_low + (volts - v_low) * (p_high - p_low) / (v_high - v_low);
        }
    }
    
    return 0.0f;
}

float battery_soc_update(float soc_prev, float v_bat, float current_A, float dt_s, float capacity_Ah)
{
	float deltaAh = current_A * (dt_s / 3600.0f);
    float delta_percent = (deltaAh / capacity_Ah) * 100.0f;

    // Si I>0 (descarga) -> SoC baja
    float soc_cc = soc_prev - delta_percent;

    if (soc_cc < 0.0f) soc_cc = 0.0f;
    if (soc_cc > 100.0f) soc_cc = 100.0f;

    // SoC por voltaje (aprox en reposo) usando la nueva LUT
    float soc_v = battery_porcent_from_voltage(v_bat);

    // Mezcla según la magnitud de la corriente
    float absI = fabsf(current_A);
    float alpha; 
    
    // Si la corriente es muy pequeña (<50mA), confiamos más en el voltaje
    if (absI < 0.05f) {        
        alpha = 0.2f;          // 20% coulomb, 80% voltaje
    } else {
        alpha = 0.8f;          // 80% coulomb, 20% voltaje
    }

    float soc = alpha * soc_cc + (1.0f - alpha) * soc_v;

    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;

    return soc;
}