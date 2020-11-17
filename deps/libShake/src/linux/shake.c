/* libShake - a basic haptic library */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "shake.h"
#include "shake_private.h"
#include "../common/helpers.h"
#include "../common/error.h"

#define SHAKE_TEST(x) ((x) ? SHAKE_TRUE : SHAKE_FALSE)

static ListElement *listHead;
static unsigned int numOfDevices;

static int nameFilter(const struct dirent *entry)
{
	const char filter[] = "event";
	return !strncmp(filter, entry->d_name, strlen(filter));
}

static void itemDelete(void *item)
{
	Shake_Device *dev = (Shake_Device *)item;

	if (!dev)
		return;

	Shake_Close(dev);

	free(dev->node);
	free(dev);
}

static Shake_Status query(Shake_Device *dev)
{
	int size = sizeof(dev->features)/sizeof(unsigned long);
	int i;

	if(!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	if (ioctl(dev->fd, EVIOCGBIT(EV_FF, sizeof(dev->features)), dev->features) == -1)
		return Shake_EmitErrorCode(SHAKE_EC_QUERY);

	for (i = 0; i < size; ++i)
	{
		if (dev->features[i])
			break;
	}

	if (i >= size) /* Device doesn't support any force feedback effects. Ignore it. */
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	if (ioctl(dev->fd, EVIOCGEFFECTS, &dev->capacity) == -1)
		return Shake_EmitErrorCode(SHAKE_EC_QUERY);

	if (dev->capacity <= 0) /* Device doesn't support uploading effects. Ignore it. */
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	if (ioctl(dev->fd, EVIOCGNAME(sizeof(dev->name)), dev->name) == -1) /* Get device name */
	{
		strncpy(dev->name, "Unknown", sizeof(dev->name));
	}

	return SHAKE_OK;
}

static Shake_Status probe(Shake_Device *dev)
{
	int isHaptic;

	if(!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);
	if (!dev->node)
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	dev->fd = open(dev->node, O_RDWR);

	if (!dev->fd)
		return SHAKE_ERROR;

	isHaptic = !query(dev);
	dev->fd = close(dev->fd);

	return isHaptic ? SHAKE_OK : SHAKE_ERROR;
}

/* API implementation. */

Shake_Status Shake_Init(void)
{
	struct dirent **nameList;
	int numOfEntries;

	numOfDevices = 0;

	numOfEntries = scandir(SHAKE_DIR_NODES, &nameList, nameFilter, alphasort);

	if (numOfEntries < 0)
	{
		return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
	}
	else
	{
		int i;

		for (i = 0; i < numOfEntries; ++i)
		{
			Shake_Device dev;

			dev.node = malloc(strlen(SHAKE_DIR_NODES) + strlen("/") + strlen(nameList[i]->d_name) + 1);
			if (dev.node == NULL)
			{
				return Shake_EmitErrorCode(SHAKE_EC_DEVICE);
			}

			strcpy(dev.node, SHAKE_DIR_NODES);
			strcat(dev.node, "/");
			strcat(dev.node, nameList[i]->d_name);

			if (probe(&dev) == SHAKE_OK)
			{
				dev.id = numOfDevices;
				listHead = listElementPrepend(listHead);
				listHead->item = malloc(sizeof(Shake_Device));
				memcpy(listHead->item, &dev, sizeof(Shake_Device));
				++numOfDevices;
			}
			else
			{
				free(dev.node);
			}

			free(nameList[i]);
		}

		free(nameList);
	}

	return SHAKE_OK;
}

void Shake_Quit(void)
{
	if (listHead != NULL)
	{
		listElementDeleteAll(listHead, itemDelete);
	}

	listHead     = NULL;
	numOfDevices = 0;
}

int Shake_NumOfDevices(void)
{
	return numOfDevices;
}

Shake_Device *Shake_Open(unsigned int id)
{
	Shake_Device *dev;
	ListElement *element;

	if (id >= numOfDevices)
	{
		Shake_EmitErrorCode(SHAKE_EC_ARG);
		return NULL;
	}

	element = listElementGet(listHead, numOfDevices - 1 - id);
	dev = (Shake_Device *)element->item;

	if(!dev || !dev->node)
	{
		Shake_EmitErrorCode(SHAKE_EC_DEVICE);
		return NULL;
	}

	dev->fd = open(dev->node, O_RDWR);
	if (!dev->fd)
		Shake_EmitErrorCode(SHAKE_EC_DEVICE);

	return dev->fd ? dev : NULL;
}

int Shake_DeviceId(Shake_Device *dev)
{
	if (!dev)
		Shake_EmitErrorCode(SHAKE_EC_ARG);

	return dev ? dev->id : SHAKE_ERROR;
}

const char *Shake_DeviceName(Shake_Device *dev)
{
	if (!dev)
		Shake_EmitErrorCode(SHAKE_EC_ARG);

	return dev ? dev->name : NULL;
}

int Shake_DeviceEffectCapacity(Shake_Device *dev)
{
	if (!dev)
		Shake_EmitErrorCode(SHAKE_EC_ARG);

	return dev ? dev->capacity : SHAKE_ERROR;
}

Shake_Bool Shake_QueryEffectSupport(Shake_Device *dev, Shake_EffectType type)
{
	/* Starts at a magic, non-zero number, FF_RUMBLE.
	   Increments respectively to EffectType. */
	return SHAKE_TEST(test_bit(FF_RUMBLE + type, dev->features));
}

Shake_Bool Shake_QueryWaveformSupport(Shake_Device *dev, Shake_PeriodicWaveform waveform)
{
	/* Starts at a magic, non-zero number, FF_SQUARE.
	   Increments respectively to PeriodicWaveform. */
	return SHAKE_TEST(test_bit(FF_SQUARE + waveform, dev->features));
}

Shake_Bool Shake_QueryGainSupport(Shake_Device *dev)
{
	return SHAKE_TEST(test_bit(FF_GAIN, dev->features));
}

Shake_Bool Shake_QueryAutocenterSupport(Shake_Device *dev)
{
	return SHAKE_TEST(test_bit(FF_AUTOCENTER, dev->features));
}

Shake_Status Shake_SetGain(Shake_Device *dev, int gain)
{
	struct input_event ie;

	if (!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	if (gain < 0)
		gain = 0;
	if (gain > 100)
		gain = 100;

	ie.type = EV_FF;
	ie.code = FF_GAIN;
	ie.value = 0xFFFFUL * gain / 100;

	if (write(dev->fd, &ie, sizeof(ie)) == -1)
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);

	return SHAKE_OK;
}

Shake_Status Shake_SetAutocenter(Shake_Device *dev, int autocenter)
{
	struct input_event ie;

	if (!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	if (autocenter < 0)
		autocenter = 0;
	if (autocenter > 100)
		autocenter = 100;

	ie.type = EV_FF;
	ie.code = FF_AUTOCENTER;
	ie.value = 0xFFFFUL * autocenter / 100;

	if (write(dev->fd, &ie, sizeof(ie)) == -1)
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);

	return SHAKE_OK;
}

Shake_Status Shake_InitEffect(Shake_Effect *effect, Shake_EffectType type)
{
	if (!effect || type >= SHAKE_EFFECT_COUNT)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	memset(effect, 0, sizeof(*effect));
	effect->type = type;
	effect->id = -1;

	return SHAKE_OK;
}

int Shake_UploadEffect(Shake_Device *dev, Shake_Effect *effect)
{
	struct ff_effect e;

	if (!dev || !effect || effect->id < -1)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	/* Common parameters. */
	e.id = effect->id;
	e.direction = effect->direction;
	e.trigger.button = 0;
	e.trigger.interval = 0;
	e.replay.delay = effect->delay;
	e.replay.length = effect->length;

	/* Effect type specific parameters. */
	if(effect->type == SHAKE_EFFECT_RUMBLE)
	{
		e.type = FF_RUMBLE;
		e.u.rumble.strong_magnitude = effect->u.rumble.strongMagnitude;
		e.u.rumble.weak_magnitude = effect->u.rumble.weakMagnitude;
	}
	else if(effect->type == SHAKE_EFFECT_PERIODIC)
	{
		e.type = FF_PERIODIC;
		e.u.periodic.waveform = FF_SQUARE + effect->u.periodic.waveform;
		e.u.periodic.period = effect->u.periodic.period;
		e.u.periodic.magnitude = effect->u.periodic.magnitude;
		e.u.periodic.offset = effect->u.periodic.offset;
		e.u.periodic.phase = effect->u.periodic.phase;
		e.u.periodic.envelope.attack_length = effect->u.periodic.envelope.attackLength;
		e.u.periodic.envelope.attack_level = effect->u.periodic.envelope.attackLevel;
		e.u.periodic.envelope.fade_length = effect->u.periodic.envelope.fadeLength;
		e.u.periodic.envelope.fade_level = effect->u.periodic.envelope.fadeLevel;
	}
	else if(effect->type == SHAKE_EFFECT_CONSTANT)
	{
		e.type = FF_CONSTANT;
		e.u.constant.level = effect->u.constant.level;
		e.u.constant.envelope.attack_length = effect->u.constant.envelope.attackLength;
		e.u.constant.envelope.attack_level = effect->u.constant.envelope.attackLevel;
		e.u.constant.envelope.fade_length = effect->u.constant.envelope.fadeLength;
		e.u.constant.envelope.fade_level = effect->u.constant.envelope.fadeLevel;
	}
	else if(effect->type == SHAKE_EFFECT_RAMP)
	{
		e.type = FF_RAMP;
		e.u.ramp.start_level = effect->u.ramp.startLevel;
		e.u.ramp.end_level = effect->u.ramp.endLevel;
		e.u.ramp.envelope.attack_length = effect->u.ramp.envelope.attackLength;
		e.u.ramp.envelope.attack_level = effect->u.ramp.envelope.attackLevel;
		e.u.ramp.envelope.fade_length = effect->u.ramp.envelope.fadeLength;
		e.u.ramp.envelope.fade_level = effect->u.ramp.envelope.fadeLevel;
	}
	else
	{
		return (effect->type >= SHAKE_EFFECT_COUNT ? Shake_EmitErrorCode(SHAKE_EC_ARG) : Shake_EmitErrorCode(SHAKE_EC_SUPPORT));
	}

	if (ioctl(dev->fd, EVIOCSFF, &e) == -1)
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	return e.id;
}

Shake_Status Shake_EraseEffect(Shake_Device *dev, int id)
{
	if (!dev || id < 0)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	if (ioctl(dev->fd, EVIOCRMFF, id) == -1)
	{
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);
	}

	return SHAKE_OK;
}

Shake_Status Shake_Play(Shake_Device *dev, int id)
{
	struct input_event play;
	if (!dev || id < 0)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	play.type = EV_FF;
	play.code = id; /* the id we got when uploading the effect */
	play.value = FF_STATUS_PLAYING; /* play: FF_STATUS_PLAYING, stop: FF_STATUS_STOPPED */

	if (write(dev->fd, (const void*) &play, sizeof(play)) == -1)
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);

	return SHAKE_OK;
}

Shake_Status Shake_Stop(Shake_Device *dev, int id)
{
	struct input_event stop;
	if (!dev || id < 0)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	stop.type = EV_FF;
	stop.code = id; /* the id we got when uploading the effect */
	stop.value = FF_STATUS_STOPPED;

	if (write(dev->fd, (const void*) &stop, sizeof(stop)) == -1)
		return Shake_EmitErrorCode(SHAKE_EC_TRANSFER);

	return SHAKE_OK;
}

Shake_Status Shake_Close(Shake_Device *dev)
{
	if (!dev)
		return Shake_EmitErrorCode(SHAKE_EC_ARG);

	close(dev->fd);

	return SHAKE_OK;
}
