#include <shake.h>
#include <stdio.h>
#include <unistd.h>

void deviceInfo(Shake_Device *device)
{
	printf("\nDevice #%d\n", Shake_DeviceId(device));
	printf(" Name: %s\n", Shake_DeviceName(device));
	printf(" Adjustable gain: %s\n", Shake_QueryGainSupport(device) ? "yes" : "no");
	printf(" Adjustable autocenter: %s\n", Shake_QueryAutocenterSupport(device) ? "yes" : "no");
	printf(" Effect capacity: %d\n", Shake_DeviceEffectCapacity(device));
	printf(" Supported effects:\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_RUMBLE)) printf(" SHAKE_EFFECT_RUMBLE\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_PERIODIC))
	{
		printf(" SHAKE_EFFECT_PERIODIC\n");
		if (Shake_QueryWaveformSupport(device, SHAKE_PERIODIC_SQUARE)) printf(" * SHAKE_PERIODIC_SQUARE\n");
		if (Shake_QueryWaveformSupport(device, SHAKE_PERIODIC_TRIANGLE)) printf(" * SHAKE_PERIODIC_TRIANGLE\n");
		if (Shake_QueryWaveformSupport(device, SHAKE_PERIODIC_SINE)) printf(" * SHAKE_PERIODIC_SINE\n");
		if (Shake_QueryWaveformSupport(device, SHAKE_PERIODIC_SAW_UP)) printf(" * SHAKE_PERIODIC_SAW_UP\n");
		if (Shake_QueryWaveformSupport(device, SHAKE_PERIODIC_SAW_DOWN)) printf(" * SHAKE_PERIODIC_SAW_DOWN\n");
		if (Shake_QueryWaveformSupport(device, SHAKE_PERIODIC_CUSTOM)) printf(" * SHAKE_PERIODIC_CUSTOM\n");
	}
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_CONSTANT)) printf(" SHAKE_EFFECT_CONSTANT\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_SPRING)) printf(" SHAKE_EFFECT_SPRING\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_FRICTION)) printf(" SHAKE_EFFECT_FRICTION\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_DAMPER)) printf(" SHAKE_EFFECT_DAMPER\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_INERTIA)) printf(" SHAKE_EFFECT_INERTIA\n");
	if (Shake_QueryEffectSupport(device, SHAKE_EFFECT_RAMP)) printf(" SHAKE_EFFECT_RAMP\n");
}

int main()
{
	int numDevices;
	int i;

	Shake_Init();
	numDevices = Shake_NumOfDevices();
	printf("Detected devices: %d\n", numDevices);

	for (i = 0; i < numDevices; ++i)
	{
		Shake_Device *device = Shake_Open(i);
		deviceInfo(device);
		Shake_Close(device);
	}

	Shake_Quit();

	return 0;
}
