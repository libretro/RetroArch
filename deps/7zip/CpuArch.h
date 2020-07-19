/* CpuArch.h -- CPU specific code
2010-10-26: Igor Pavlov : Public domain */

#ifndef __CPU_ARCH_H
#define __CPU_ARCH_H

#include <retro_endianness.h>
#include "7zTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MY_CPU_LE_UNALIGN means that CPU is LITTLE ENDIAN and 
   CPU supports unaligned memory accesses.
   If MY_CPU_LE_UNALIGN is not defined, we don't know 
   about these properties of platform.
*/

#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
#define MY_CPU_AMD64
#endif

#if defined(MY_CPU_AMD64) || defined(_M_IA64)
#define MY_CPU_64BIT
#endif

#if defined(_M_IX86) || defined(__i386__)
#define MY_CPU_X86
#endif

#if defined(MY_CPU_X86) || defined(MY_CPU_AMD64)
#define MY_CPU_X86_OR_AMD64
#endif

#if defined(MY_CPU_X86) || defined(_M_ARM)
#define MY_CPU_32BIT
#endif

#if defined(_WIN32) && defined(_M_ARM)
#define MY_CPU_ARM_LE
#endif

#if defined(_WIN32) && defined(_M_IA64)
#define MY_CPU_IA64_LE
#endif

#if defined(MY_CPU_X86_OR_AMD64)
#define MY_CPU_LE_UNALIGN
#endif

#ifdef __cplusplus
}
#endif

#endif
