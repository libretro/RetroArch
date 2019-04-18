#ifndef __DEFINITIONS_H__
#define __DEFINITIONS_H__

#include "os.h"

#define WIIMOTE_PI			3.14159265f

//#define WITH_WIIUSE_DEBUG

/* Error output macros */
#define WIIUSE_ERROR(fmt, ...)		fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

/* Warning output macros */
#define WIIUSE_WARNING(fmt, ...)	fprintf(stderr, "[WARNING] " fmt "\n",	##__VA_ARGS__)

/* Information output macros */
#define WIIUSE_INFO(fmt, ...)		fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)

#ifdef WITH_WIIUSE_DEBUG
	#ifdef WIN32
		#define WIIUSE_DEBUG(fmt, ...)		do {																				\
												char* file = __FILE__;															\
												int i = strlen(file) - 1;														\
												for (; i && (file[i] != '\\'); --i);											\
												fprintf(stderr, "[DEBUG] %s:%i: " fmt "\n", file+i+1, __LINE__, ##__VA_ARGS__);	\
											} while (0)
	#else
		#define WIIUSE_DEBUG(fmt, ...)	fprintf(stderr, "[DEBUG] " __FILE__ ":%i: " fmt "\n", __LINE__, ##__VA_ARGS__)
	#endif
#else
	#define WIIUSE_DEBUG(fmt, ...)
#endif

#if 1
#define WII_DEBUG(fmt, ...)			do {																						\
										printf("[WDEBUG] " __FILE__ ":%i: " fmt "\n", __LINE__, ##__VA_ARGS__);					\
										usleep(3000000);																		\
									} while (0)
#else
	#define WII_DEBUG(fmt, ...)
#endif

/* Convert between radians and degrees */
#define RAD_TO_DEGREE(r)	((r * 180.0f) / WIIMOTE_PI)
#define DEGREE_TO_RAD(d)	(d * (WIIMOTE_PI / 180.0f))

/* Convert to big endian */
#define BIG_ENDIAN_LONG(i)				(htonl(i))
#define BIG_ENDIAN_SHORT(i)				(htons(i))

#define absf(x)						((x >= 0) ? (x) : (x * -1.0f))
#define diff_f(x, y)				((x >= y) ? (absf(x - y)) : (absf(y - x)))

#endif
