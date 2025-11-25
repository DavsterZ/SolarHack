#include "battery.h"

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