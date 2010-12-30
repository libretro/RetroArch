#ifndef __FILTERS_H
#define __FILTERS_H

#ifdef HAVE_FILTER

#include "pastlib.h"
#include "grayscale.h"
#include "bleed.h"
#include "ntsc.h"

#define FILTER_HQ2X 1
#define FILTER_HQ4X 2
#define FILTER_GRAYSCALE 3
#define FILTER_BLEED 4
#define FILTER_NTSC 5
#define FILTER_HQ2X_STR "hq2x"
#define FILTER_HQ4X_STR "hq4x"
#define FILTER_GRAYSCALE_STR "grayscale"
#define FILTER_BLEED_STR "bleed"
#define FILTER_NTSC_STR "ntsc"
#endif

#endif
