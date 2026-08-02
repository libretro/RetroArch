// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

// This file needs to compile in C and C++ context, so deliberately broken out.

#ifndef RPI_GPIOBITS_H
#define RPI_GPIOBITS_H

#include <stdint.h>
#ifdef ENABLE_WIDE_GPIO_COMPUTE_MODULE
typedef uint64_t gpio_bits_t;
#else
typedef uint32_t gpio_bits_t;
#endif

#endif
