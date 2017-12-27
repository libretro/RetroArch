#ifndef __LED_DRIVER__H
#define __LED_DRIVER__H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <libretro.h>

#include "../msg_hash.h"

RETRO_BEGIN_DECLS

typedef struct led_driver
{
    void (*init)(void);
    void (*free)(void);
    void (*set_led)(int led,int value);
} led_driver_t;

bool led_driver_init(void);

void led_driver_free(void);

void led_driver_set_led(int led,int value);

#endif
