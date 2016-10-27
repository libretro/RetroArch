#ifndef __UTILS_H_
#define __UTILS_H_

#include <malloc.h>
#include "../common/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FlushBlock(addr)   asm volatile("dcbf %0, %1\n"                                \
                                        "icbi %0, %1\n"                                \
                                        "sync\n"                                       \
                                        "eieio\n"                                      \
                                        "isync\n"                                      \
                                        :                                              \
                                        :"r"(0), "r"(((addr) & ~31))                   \
                                        :"memory", "ctr", "lr", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"     \
                                        );

#define LIMIT(x, min, max)																	\
	({																						\
		typeof( x ) _x = x;																	\
		typeof( min ) _min = min;															\
		typeof( max ) _max = max;															\
		( ( ( _x ) < ( _min ) ) ? ( _min ) : ( ( _x ) > ( _max ) ) ? ( _max) : ( _x ) );	\
	})

#define DegToRad(a)   ( (a) *  0.01745329252f )
#define RadToDeg(a)   ( (a) * 57.29577951f )

#define ALIGN4(x)           (((x) + 3) & ~3)
#define ALIGN32(x)          (((x) + 31) & ~31)

// those work only in powers of 2
#define ROUNDDOWN(val, align)   ((val) & ~(align-1))
#define ROUNDUP(val, align)     ROUNDDOWN(((val) + (align-1)), align)

#define le16(i)         ((((u16) ((i) & 0xFF)) << 8) | ((u16) (((i) & 0xFF00) >> 8)))
#define le32(i)         ((((u32)le16((i) & 0xFFFF)) << 16) | ((u32)le16(((i) & 0xFFFF0000) >> 16)))
#define le64(i)         ((((u64)le32((i) & 0xFFFFFFFFLL)) << 32) | ((u64)le32(((i) & 0xFFFFFFFF00000000LL) >> 32)))

#ifdef __cplusplus
}
#endif

#endif // __UTILS_H_
