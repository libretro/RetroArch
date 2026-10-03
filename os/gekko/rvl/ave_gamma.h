/* The A/V encoder's gamma curve, registers 0x10-0x30: six segment
 * slopes (4.12 fixed point), seven input knees and the outputs at
 * them (8.8 fixed point), big endian, over the 16-235 video range. */

#ifndef GEKKO_RVL_AVE_GAMMA_H
#define GEKKO_RVL_AVE_GAMMA_H

#include <stdint.h>

#define AVE_GAMMA_LEN 33

/* gamma in tenths: 10 is 1.0, the linear curve; 1 to 30. */
void ave_gamma_table(unsigned tenths, uint8_t out[AVE_GAMMA_LEN]);

#endif
