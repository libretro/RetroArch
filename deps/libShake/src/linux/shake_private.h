#ifndef _SHAKE_PRIVATE_H_
#define _SHAKE_PRIVATE_H_

#include <dirent.h>
#include <linux/input.h>
#include "../common/helpers.h"

#define SHAKE_DIR_NODES		"/dev/input"

#define BITS_PER_LONG		(sizeof(long) * 8)
#define OFF(x)			((x)%BITS_PER_LONG)
#define BIT(x)			(1UL<<OFF(x))
#define LONG(x)			((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)
#define BITS_TO_LONGS(x) \
	(((x) + 8 * sizeof (unsigned long) - 1) / (8 * sizeof (unsigned long)))

struct Shake_Device
{
	char name[128];
	int id;
	int capacity; /* Number of effects the device can play at the same time */
	/* Platform dependent section */
	int fd;
	char *node;
	unsigned long features[BITS_TO_LONGS(FF_CNT)];

};

#endif /* _SHAKE_PRIVATE_H_ */
