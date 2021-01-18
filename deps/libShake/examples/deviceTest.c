#include <shake.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

Shake_Device *device;
static int quit;

static void waitForKey(void)
{
	printf("Press RETURN to continue");
	while(getchar() != '\n');
}

static void listDevices(void)
{
	int numDevices;
	int i;

	numDevices = Shake_NumOfDevices();
	printf("Detected devices: %d\n\n", numDevices);

	for (i = 0; i < numDevices; ++i)
	{
		Shake_Device *dev = (i == Shake_DeviceId(dev)) ? dev : Shake_Open(i);
		printf("#%2d: %s\n", Shake_DeviceId(dev), Shake_DeviceName(dev));

		if (dev != device)
			Shake_Close(dev);
	}
}

static void deviceInfo(void)
{
	printf("\nDevice #%d\n", Shake_DeviceId(device));
	printf(" Name:                   %s\n", Shake_DeviceName(device));
	printf(" Adjustable gain:        %s\n", Shake_QueryGainSupport(device) ? "yes" : "no");
	printf(" Adjustable autocenter:  %s\n", Shake_QueryAutocenterSupport(device) ? "yes" : "no");
	printf(" Effect capacity:        %d\n", Shake_DeviceEffectCapacity(device));
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

	printf("\n");
}

static void testCapacity(void)
{
	Shake_Effect effect;
	int capacity = Shake_DeviceEffectCapacity(device);
	int *id;
	int i;

	id = malloc(capacity * sizeof(int));
	if (!id)
	{
		printf("Unable to allocate memory.\n");
		return;
	}

	printf("-Capacity-\n");
	printf("Reported capacity: %d\n", capacity);

	for (i = 0; i < capacity; ++i)
	{
		Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SINE, 1.0, 0.0, 1.0, 0.0);
		id[i] = Shake_UploadEffect(device, &effect);
		printf("Uploaded #%d as #%d\n", i, id[i]);
	}

	printf("-End-\n\n");

	for (i = 0; i < capacity; ++i)
	{
		Shake_EraseEffect(device, id[i]);
	}

	free(id);
}

static void testEffectPlayback(void)
{
	Shake_Effect effect;
	int id;

	printf("-Effect playback-\n");
	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SINE, 1.0, 0.0, 2.0, 0.0);
	id = Shake_UploadEffect(device, &effect);
	Shake_Play(device, id);
	printf("Playing (2 sec)\n");
	sleep(1);
	Shake_Stop(device, id);
	printf("Stopping (at 1 sec)\n");
	sleep(1);
	Shake_Play(device, id);
	printf("Replaying (2 sec)\n");
	sleep(2);
	printf("-End-\n\n");

	Shake_EraseEffect(device, id);
}

static void testEffectOrder(void)
{
	Shake_Effect effect;
	int id[4];
	int shuffle[4];
	int i;

	printf("-Effect order-\n");
	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SINE, 1.0, 0.0, 1.0, 0.0);
	id[0] = Shake_UploadEffect(device, &effect);

	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SQUARE, 1.0, 0.0, 1.0, 0.0);
	id[1] = Shake_UploadEffect(device, &effect);

	Shake_SimpleRumble(&effect, 1.0, 1.0, 1.0);
	id[2] = Shake_UploadEffect(device, &effect);

	Shake_SimpleRumble(&effect, 0.8, 1.0, 1.0);
	id[3] = Shake_UploadEffect(device, &effect);

	for (i = 0; i < 4; ++i)
	{
		int r;

		while (1)
		{
			int repeat = 0;
			int j;

			r = rand() % 4;
			for (j = 0; j < i; ++j)
			{
				if (r == shuffle[j])
				{
					repeat = 1;
					break;
				}
			}

			if (!repeat)
				break;
		}

		shuffle[i] = r;
	}

	for (i = 0; i < 4; ++i)
	{
		Shake_Play(device, id[shuffle[i]]);
		printf("Effect #%d\n", shuffle[i]);
		sleep(1);
	}
	printf("-End-\n\n");

	for (i = 0; i < 4; ++i)
	{
		Shake_EraseEffect(device, id[i]);
	}
}

static void testEffectUpdate(void)
{
	Shake_Effect effect;
	int id;

	printf("-Effect update-\n");
	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SINE, 1.0, 0.0, 4.0, 0.0);
	id = Shake_UploadEffect(device, &effect);
	Shake_Play(device, id);
	printf("Original\n");
	sleep(2);
	effect.u.periodic.magnitude = 0x6000;
	effect.id = id;
	id = Shake_UploadEffect(device, &effect);
	printf("Updated\n");
	sleep(2);
	printf("-End-\n\n");

	Shake_EraseEffect(device, id);
}

static void testEffectMixing(void)
{
	Shake_Effect effect;
	int id[3];
	int i;

	printf("-Effect mixing-\n");
	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SINE, 0.6, 0.0, 4.0, 0.0);
	id[0] = Shake_UploadEffect(device, &effect);
	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SQUARE, 0.2, 0.0, 2.0, 0.0);
	id[1] = Shake_UploadEffect(device, &effect);
	Shake_SimplePeriodic(&effect, SHAKE_PERIODIC_SINE, 0.2, 0.0, 1.0, 0.0);
	id[2] = Shake_UploadEffect(device, &effect);

	Shake_Play(device, id[0]);
	printf("Playing #1 (0.6 mag)\n");
	sleep(1);
	Shake_Play(device, id[1]);
	printf("Adding #2 (+0.2 mag)\n");
	sleep(1);
	Shake_Play(device, id[2]);
	printf("Adding #3 (+0.2 mag)\n");
	sleep(1);
	printf("Removing #2 and #3 (-0.4 mag)\n");
	sleep(1);
	printf("-End-\n\n");

	for (i = 0; i < 3; ++i)
	{
		Shake_EraseEffect(device, id[i]);
	}
}

static void menu(void)
{
	int selection;

	printf("Device #%d: %s\n\n", Shake_DeviceId(device), Shake_DeviceName(device));
	printf("Select test:\n");
	printf("1) Effect capacity\n2) Effect playback\n3) Effect order\n4) Effect update\n5) Effect mixing\n6) Play all above tests\n\nI) Device info\nL) List devices\nQ) Quit\n\n");
	printf("> ");

	selection = getchar();

	switch (selection)
	{
		case 'q':
		case 'Q':
			quit = 1;
		break;
		case 'i':
		case 'I':
			deviceInfo();
			waitForKey();
		break;
		case 'l':
		case 'L':
			listDevices();
			waitForKey();
		break;
		case '1':
			testCapacity();
			waitForKey();
		break;
		case '2':
			testEffectPlayback();
			waitForKey();
		break;
		case '3':
			testEffectOrder();
			waitForKey();
		break;
		case '4':
			testEffectUpdate();
			waitForKey();
		break;
		case '5':
			testEffectMixing();
			waitForKey();
		break;
		case '6':
			testCapacity();
			sleep(1);
			testEffectPlayback();
			sleep(1);
			testEffectOrder();
			sleep(1);
			testEffectUpdate();
			sleep(1);
			testEffectMixing();
			waitForKey();
		break;

		default:
		break;
	}

	while(getchar() != '\n');
	printf("------\n");
}

int main(int argc, const char *argv[])
{
	int deviceId = 0;

	if (argc > 1)
		deviceId = atoi(argv[1]);

	if (deviceId < 0)
	{
		printf("Device number must be greater than or equal to 0.\n");
		return -1;
	}

	srand(time(NULL));
	Shake_Init();

	if (Shake_NumOfDevices() <= deviceId)
	{
		printf("Device #%d doesn't exist.\n", deviceId);
		return -1;
	}

	device = Shake_Open(deviceId);

	while (!quit)
		menu();

	/* Cleanup. */
	Shake_Close(device);
	Shake_Quit();

	return 0;
}
