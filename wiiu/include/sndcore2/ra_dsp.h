#pragma once

#include <wut.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct DspConfig DspConfig;

struct DspConfig {
    WUT_UNKNOWN_BYTES(74);
    uint16_t multi_ch_count;
    WUT_UNKNOWN_BYTES(20);
};

WUT_CHECK_OFFSET(DspConfig, 0x4a, multi_ch_count);
WUT_CHECK_SIZE(DspConfig, 0x60);

#ifdef __cplusplus
}
#endif
