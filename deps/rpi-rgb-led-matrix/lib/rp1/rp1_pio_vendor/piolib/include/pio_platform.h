// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Raspberry Pi Ltd.
 * All rights reserved.
 */

#ifndef _PIO_PLATFORM_H
#define _PIO_PLATFORM_H

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define PICO_DEFAULT_LED_PIN 4

#ifndef PARAM_ASSERTIONS_ENABLE_ALL
#define PARAM_ASSERTIONS_ENABLE_ALL 0
#endif

#ifndef PARAM_ASSERTIONS_DISABLE_ALL
#define PARAM_ASSERTIONS_DISABLE_ALL 0
#endif

#define PARAM_ASSERTIONS_ENABLED(x) ((PARAM_ASSERTIONS_ENABLED_ ## x || PARAM_ASSERTIONS_ENABLE_ALL) && !PARAM_ASSERTIONS_DISABLE_ALL)
#define invalid_params_if(x, test) ({if (PARAM_ASSERTIONS_ENABLED(x)) assert(!(test));})
#define valid_params_if(x, test) ({if (PARAM_ASSERTIONS_ENABLED(x)) assert(test);})

#define STATIC_ASSERT(cond) static_assert(cond, #cond)

#define _u(x) ((uint)(x))
#define bool_to_bit(x) ((uint)!!(x))

#ifndef count_of
#define count_of(a) (sizeof(a)/sizeof((a)[0]))
#endif

typedef unsigned int uint;

#endif
