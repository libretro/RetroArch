/* <bits/floatn.h> for the PS4/PS5 lanes, which borrow the host's glibc
 * headers. glibc from 2.43 offers __float128 to any clang, and clang's
 * SCE targets have no such type: neither has the consoles' own libc.
 * This is glibc's header for a target without binary128. */
#ifndef _BITS_FLOATN_H
#define _BITS_FLOATN_H

#include <features.h>

#define __HAVE_FLOAT128 0
#define __HAVE_DISTINCT_FLOAT128 0
#define __HAVE_FLOAT64X 1
#define __HAVE_FLOAT64X_LONG_DOUBLE 1

#include <bits/floatn-common.h>

#endif
