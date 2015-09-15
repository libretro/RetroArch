#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <stdint.h>
#include <sys/cdefs.h>

#include <retro_log.h>

typedef enum
{
   CPU_FAMILY_UNKNOWN = 0,
   CPU_FAMILY_ARM,
   CPU_FAMILY_X86,
   CPU_FAMILY_MIPS,

   CPU_FAMILY_MAX  /* do not remove */
} cpu_family;

/* Return family of the device's CPU */
extern cpu_family   android_getCpuFamily(void);

enum
{
   CPU_ARM_FEATURE_ARMv7       = (1 << 0),
   CPU_ARM_FEATURE_VFPv3       = (1 << 1),
   CPU_ARM_FEATURE_NEON        = (1 << 2),
   CPU_ARM_FEATURE_LDREX_STREX = (1 << 3)
};

enum
{
   CPU_X86_FEATURE_SSSE3  = (1 << 0),
   CPU_X86_FEATURE_POPCNT = (1 << 1),
   CPU_X86_FEATURE_MOVBE  = (1 << 2)
};

extern uint64_t    android_getCpuFeatures(void);

/* Return the number of CPU cores detected on this device. */
extern int         android_getCpuCount(void);

#endif /* CPU_FEATURES_H */
