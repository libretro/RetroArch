/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
 ***************************************************************************

    minmax.h

***************************************************************************/

#pragma once

#ifndef __MINMAX_H__
#define __MINMAX_H__

#if defined(RARCH_INTERNAL) || defined(__LIBRETRO__)
#include <retro_miscellaneous.h>
#else
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#endif
