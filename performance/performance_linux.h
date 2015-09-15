#ifndef PERFORMANCE_LINUX_H 
#define PERFORMANCE_LINUX_H 

#include <stdint.h>
#include <sys/cdefs.h>

typedef enum
{
   CPU_FAMILY_UNKNOWN = 0,
   CPU_FAMILY_ARM,
   CPU_FAMILY_X86,
   CPU_FAMILY_MIPS,

   CPU_FAMILY_MAX  /* do not remove */
} cpu_family;

enum
{
   CPU_ARM_FEATURE_ARMv7       = (1 << 0),
   CPU_ARM_FEATURE_VFPv3       = (1 << 1),
   CPU_ARM_FEATURE_NEON        = (1 << 2),
   CPU_ARM_FEATURE_LDREX_STREX = (1 << 3)
};

enum
{
   CPU_X86_FEATURE_SSSE3       = (1 << 0),
   CPU_X86_FEATURE_POPCNT      = (1 << 1),
   CPU_X86_FEATURE_MOVBE       = (1 << 2)
};

cpu_family   linux_get_cpu_family(void);

uint64_t    linux_get_cpu_features(void);

int         linux_get_cpu_count(void);

#endif
