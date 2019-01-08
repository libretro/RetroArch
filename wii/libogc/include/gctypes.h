#ifndef __GCTYPES_H__
#define __GCTYPES_H__

/*! \file gctypes.h
\brief Data type definitions

*/

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

/*+----------------------------------------------------------------------------------------------+*/
typedef uint8_t u8;					///< 8bit unsigned integer
typedef uint16_t u16;				///< 16bit unsigned integer
typedef uint32_t u32;				///< 32bit unsigned integer
typedef uint64_t u64;				///< 64bit unsigned integer
/*+----------------------------------------------------------------------------------------------+*/
typedef int8_t s8;					///< 8bit signed integer
typedef int16_t s16;				///< 16bit signed integer
typedef int32_t s32;				///< 32bit signed integer
typedef int64_t s64;				///< 64bit signed integer
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile u8 vu8;			///< 8bit unsigned volatile integer
typedef volatile u16 vu16;			///< 16bit unsigned volatile integer
typedef volatile u32 vu32;			///< 32bit unsigned volatile integer
typedef volatile u64 vu64;			///< 64bit unsigned volatile integer
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile s8 vs8;			///< 8bit signed volatile integer
typedef volatile s16 vs16;			///< 16bit signed volatile integer
typedef volatile s32 vs32;			///< 32bit signed volatile integer
typedef volatile s64 vs64;			///< 64bit signed volatile integer
/*+----------------------------------------------------------------------------------------------+*/
// fixed point math typedefs
typedef s16 sfp16;					///< signed 8:8 fixed point
typedef s32 sfp32;					///< signed 20:8 fixed point
typedef u16 ufp16;					///< unsigned 8:8 fixed point
typedef u32 ufp32;					///< unsigned 24:8 fixed point
/*+----------------------------------------------------------------------------------------------+*/
typedef float f32;
typedef double f64;
/*+----------------------------------------------------------------------------------------------+*/
typedef volatile float vf32;
typedef volatile double vf64;
/*+----------------------------------------------------------------------------------------------+*/

typedef unsigned int BOOL;
/*+----------------------------------------------------------------------------------------------+*/
// alias type typedefs
#define FIXED s32					///< Alias type for sfp32
/*+----------------------------------------------------------------------------------------------+*/
#ifndef TRUE
#define TRUE	1					///< True
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef FALSE
#define FALSE	0					///< False
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef NULL
#define NULL	0					///< Pointer to 0
#endif
/*+----------------------------------------------------------------------------------------------+*/
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN  3412
#endif /* LITTLE_ENDIAN */
/*+----------------------------------------------------------------------------------------------+*/
#ifndef BIG_ENDIAN
#define BIG_ENDIAN     1234
#endif /* BIGE_ENDIAN */
/*+----------------------------------------------------------------------------------------------+*/
#ifndef BYTE_ORDER
#define BYTE_ORDER     BIG_ENDIAN
#endif /* BYTE_ORDER */
/*+----------------------------------------------------------------------------------------------+*/

//!	argv structure
/*!	\struct __argv

	structure used to set up argc/argv

*/
struct __argv {
	int argvMagic;		//!< argv magic number, set to 0x5f617267 ('_arg') if valid
	char *commandLine;	//!< base address of command line, set of null terminated strings
	int length;//!< total length of command line
	int argc;
	char **argv;
	char **endARGV;
};

//!	Default location for the system argv structure.
extern struct __argv *__system_argv;

// argv struct magic number
#define ARGV_MAGIC 0x5f617267

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* TYPES_H */

/* END OF FILE */
