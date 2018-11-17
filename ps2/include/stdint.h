#ifndef STDINT_H
#define STDINT_H

typedef unsigned long     uintptr_t;
typedef signed long       intptr_t;

typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef signed long       int64_t;
typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;
typedef unsigned int      uint32_t;
typedef unsigned long     uint64_t;

#define	 STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

#define INT8_C(val)  val##c
#define INT16_C(val) val##h
#define INT32_C(val) val##i
#define INT64_C(val) val##l

#define UINT8_C(val)  val##uc
#define UINT16_C(val) val##uh
#define UINT32_C(val) val##ui
#define UINT64_C(val) val##ul

#endif //STDINT_H
