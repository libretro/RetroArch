// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-24 Raspberry Pi Ltd.
 * All rights reserved.
 */

#ifndef _PIOLIB_PRIV_H
#define _PIOLIB_PRIV_H

#include "pio_platform.h"

#define DECLARE_PIO_CHIP(chip) \
    const PIO_CHIP_T *__ptr_ ## chip __attribute__ ((section ("piochips"))) __attribute__ ((used)) = \
    &chip

#endif
