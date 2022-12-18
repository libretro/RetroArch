#pragma once
#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>
#include <stdint.h>

#define ALIGN(align)                 __attribute__((aligned(align)))
#define ALIGN_0x40                   ALIGN(0x40)
#define ROUNDUP(x, align)            (((x) + ((align) -1)) & ~((align) -1))

#define __FSAShimSetupRequestMount   ((FSError(*)(FSAShimBuffer *, uint32_t, const char *, const char *, uint32_t, void *, uint32_t))(0x101C400 + 0x042f88))
#define __FSAShimSetupRequestUnmount ((FSError(*)(FSAShimBuffer *, uint32_t, const char *, uint32_t))(0x101C400 + 0x43130))
#define __FSAShimSend                ((FSError(*)(FSAShimBuffer *, uint32_t))(0x101C400 + 0x042d90))
