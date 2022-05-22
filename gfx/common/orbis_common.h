#ifndef ORBIS_COMMON_H__
#define ORBIS_COMMON_H__

#ifdef HAVE_EGL
#include <piglet.h>
#include "../common/egl_common.h"
#endif

#define ATTR_ORBISGL_WIDTH 1920
#define ATTR_ORBISGL_HEIGHT 1080

#if defined(HAVE_OOSDK)
#define SIZEOF_SCE_SHDR_CACHE_CONFIG 0x10C
TYPE_BEGIN(struct _SceShdrCacheConfig, SIZEOF_SCE_SHDR_CACHE_CONFIG);
	TYPE_FIELD(uint32_t ver, 0x00);
	TYPE_FIELD(uint32_t unk1, 0x04);
	TYPE_FIELD(uint32_t unk2, 0x08);
	TYPE_FIELD(char cache_dir[128], 0x0C);
TYPE_END();
typedef struct _SceShdrCacheConfig SceShdrCacheConfig;

bool scePigletSetShaderCacheConfiguration(const SceShdrCacheConfig *config);
#endif

typedef struct
{
#ifdef HAVE_EGL
    egl_ctx_data_t egl;
    ScePglConfig pgl_config;
#if defined(HAVE_OOSDK)
    SceShdrCacheConfig shdr_cache_config;
#endif
#endif

    SceWindow native_window;
    bool resize;
    unsigned width, height;
    float refresh_rate;
} orbis_ctx_data_t;

#endif
