/* A USB Gecko in memory card slot A or B, found at boot: debug output
 * and stdout also go to it, a line at a time, dropped while the
 * computer on the other end is not reading. */

#ifndef GEKKO_USBGECKO_H
#define GEKKO_USBGECKO_H

#include <stddef.h>

/* The EXI channel it is on, or -1. */
int  gk_usbgecko_channel(void);
void gk_usbgecko_write(const char *s, size_t len);

#endif
