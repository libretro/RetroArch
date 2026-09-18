/* The frontend the surface layer calls into, reduced to what an
 * upload needs: textures are handles from a counter, and the driver
 * updates in place, which is the case the animated overlay path
 * exists for. */

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#include "../../../gfx/video_driver.h"
#include "../../../gfx/gfx_surface.h"

static uintptr_t stub_next_handle = 0x100;
static unsigned  stub_updates;

bool video_driver_texture_load(void *data, unsigned filter, uintptr_t *id)
{
   (void)data; (void)filter;
   *id = stub_next_handle++;
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   *id = 0;
   return true;
}

bool video_driver_texture_can_update(void) { return true; }

bool video_driver_texture_update(uintptr_t id, void *data)
{
   (void)data;
   stub_updates++;
   return id != 0;
}

uint32_t video_driver_get_disp_flags(void) { return 0; }

bool video_driver_test_all_flags(enum display_flags testflag)
{
   (void)testflag;
   return false;
}

bool video_driver_supports_texture_format(enum texture_gpu_format fmt)
{
   (void)fmt;
   return false;
}

enum texture_filter_type gfx_display_texture_filter(void)
{
   return TEXTURE_FILTER_LINEAR;
}
