#include "battery.h"

#include <math.h>

float battery_porcent_from_voltage(float volts)
{
	if (volts >= 4.20f) return 100.0f;
	if (volts <= 3.30f) return 0.0f;

	if (volts >= 4.10f) return 90.0f + (volts - 4.10f) * (10.0f / (4.20f - 4.10f));
	else if (volts >= 4.00f) return 80.0f + (volts - 4.00f) * (10.0f / (4.10f - 4.00f));
	else if (volts >= 3.90f) return 70.0f + (volts - 3.90f) * (10.0f / (4.00f - 3.90f));
	else if (volts >= 3.80f) return 60.0f + (volts - 3.80f) * (10.0f / (3.90f - 3.80f));
	else if (volts >= 3.75f) return 50.0f + (volts - 3.75f) * (10.0f / (3.80f - 3.75f));
	else if (volts >= 3.70f) return 40.0f + (volts - 3.70f) * (10.0f / (3.75f - 3.70f));
	else if (volts >= 3.65f) return 30.0f + (volts - 3.65f) * (10.0f / (3.70f - 3.65f));
	else if (volts >= 3.55f) return 20.0f + (volts - 3.55f) * (10.0f / (3.65f - 3.55f));
	else if (volts >= 3.55f) return 20.0f + (volts - 3.55f) * (10.0f / (3.65f - 3.55f));
	else return 10.0f + (volts - 3.45f) * (10.0f / (3.55f - 3.45f));
}

float battery_soc_update(float soc_prev, float v_bat, float current_A, float dt_s, float capacity_Ah)
{
	float deltaAh = current_A * (dt_s / 3600.0f);
    float delta_percent = (deltaAh / capacity_Ah) * 100.0f;

	// Si I>0 (descarga) -> SoC baja
    float soc_cc = soc_prev - delta_percent;

	if (soc_cc < 0.0f) soc_cc = 0.0f;
    if (soc_cc > 100.0f) soc_cc = 100.0f;

	// SoC por voltaje (aprox en reposo)
    float soc_v = battery_porcent_from_voltage(v_bat);

	// Mezcla según la magnitud de la corriente
    float absI = fabsf(current_A);

	// Si la corriente es muy pequeña (batería casi en reposo),
    // confiamos más en el voltaje. Si es grande, más en coulomb counting.
    float alpha; // peso del coulomb counting
    if (absI < 0.05f) {        // menos de 50 mA
        alpha = 0.2f;          // 20% coulomb, 80% voltaje
    } else {
        alpha = 0.8f;          // 80% coulomb, 20% voltaje
    }

	float soc = alpha * soc_cc + (1.0f - alpha) * soc_v;

    // Limitar de nuevo por seguridad
    if (soc < 0.0f) soc = 0.0f;
    if (soc > 100.0f) soc = 100.0f;

    return soc;
}