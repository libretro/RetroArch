#!/usr/bin/env python3
"""Execute the shipping menu loan and publication code with copy accounting."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_thread_wrapper.c").read_text(encoding="utf-8")
header = (root / "gfx/video_thread_wrapper.h").read_text(encoding="utf-8")
structure = re.search(r"struct video_thread_menu_texture\n\{.*?\n};", header, re.S).group()
start = source.index("static struct video_thread_menu_texture *thread_menu_texture_back(")
end = source.index("\n}\n", source.index("static void thread_set_texture_frame(", start)) + 3
functions = source[start:end]
rgui = (root / "menu/drivers/rgui.c").read_text(encoding="utf-8")
start = rgui.index("static void rgui_set_texture_frame(")
end = rgui.index("\n}\n", rgui.index("static void rgui_set_texture(", start)) + 3
render = rgui[start:end]
frontend = (root / "gfx/video_driver.c").read_text(encoding="utf-8")
start = frontend.index("void *video_driver_get_menu_framebuffer(")
frontend = frontend[start:frontend.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queues/retro_triple_buffer.h>
#include "gfx/video_defines.h"
STRUCTURE
typedef struct {
   struct {
      retro_triple_buffer_t frames;
      struct video_thread_menu_texture slot[3];
   } texture;
} thread_video_t;
static thread_video_t *video_thread_thr_capture;
static unsigned copied, allocated;
static bool active, fail_alloc;
static bool video_driver_thread_wrapper_active(void) { return active; }
static void *counted_copy(void *dst, const void *src, size_t size)
{
   copied++;
   return memcpy(dst, src, size);
}
static void *counted_realloc(void *ptr, size_t size)
{
   allocated++;
   return fail_alloc ? NULL : realloc(ptr, size);
}
#define memcpy counted_copy
#define realloc counted_realloc
FUNCTIONS
#undef memcpy
#undef realloc

#define HAVE_THREADS
#define RGUI_UPSCALE_NONE 0
#define RGUI_UPSCALE_AUTO 1
#define GFX_DISP_FLAG_FB_DIRTY 1
typedef struct { uint16_t *data; unsigned width, height; } frame_buf_t;
typedef struct { frame_buf_t frame_buf, upscale_buf; } rgui_t;
typedef struct { unsigned flags, framebuf_dims; } gfx_display_t;
typedef struct { struct { unsigned menu_rgui_internal_upscale_level; } uints; } settings_t;
struct poke {
   void (*set_texture_frame)(void*, const void*, bool, unsigned, float);
   void *(*get_menu_framebuffer)(void*, bool, unsigned, size_t*);
};
typedef struct poke video_poke_interface_t;
typedef struct { void *data; struct poke *poke; } video_driver_state_t;
static gfx_display_t display;
static settings_t settings;
static video_driver_state_t video_driver_st;
static unsigned viewport_dims;
static gfx_display_t *disp_get_ptr(void) { return &display; }
static settings_t *config_get_ptr(void) { return &settings; }
static video_driver_state_t *video_state_get_ptr(void) { return &video_driver_st; }
static void video_driver_get_viewport_info(struct video_viewport *vp)
{
   vp->dims = viewport_dims;
}
#define configuration_set_uint(s, field, value) ((field) = (value))
FRONTEND
RENDER

static uint16_t direct_pixels[256];
static unsigned direct_submits;
static void *direct_get(void *data, bool rgb32, unsigned dims, size_t *pitch)
{
   (void)data;
   assert(!rgb32 && dims == VIDEO_SCALE_PACK(8, 8));
   memset(direct_pixels, 0xcd, sizeof(direct_pixels));
   *pitch = 24;
   return direct_pixels;
}
static void direct_submit(void *data, const void *frame, bool rgb32,
      unsigned dims, float alpha)
{
   unsigned x, y;
   (void)data;
   assert(frame == direct_pixels && !rgb32 && dims == VIDEO_SCALE_PACK(8, 8));
   assert(alpha == 1.0f);
   for (y = 0; y < 8; y++)
      for (x = 0; x < 12; x++)
         assert(direct_pixels[y * 12 + x] == (x < 8
               ? (uint16_t)(10 + (y / 2) * 4 + x / 2) : 0xcdcd));
   direct_submits++;
}

static void check_upscale(thread_video_t *thr)
{
   struct poke poke;
   rgui_t rgui;
   struct video_thread_menu_texture *taken;
   uint16_t pixels[16];
   unsigned level, i, x, y, width, before;
   memset(&rgui, 0, sizeof(rgui));
   poke.set_texture_frame = thread_set_texture_frame;
   poke.get_menu_framebuffer = NULL;
   video_driver_st.poke = &poke;
   video_driver_st.data = thr;
   rgui.frame_buf.data = pixels;
   for (i = 0; i < 16; i++) pixels[i] = (uint16_t)(i + 10);
   display.framebuf_dims = VIDEO_SCALE_PACK(4, 4);
   viewport_dims = VIDEO_SCALE_PACK(8, 8);
   for (level = 1; level <= 3; level++)
   {
      settings.uints.menu_rgui_internal_upscale_level = level;
      display.flags = GFX_DISP_FLAG_FB_DIRTY;
      before = copied;
      rgui_set_texture(&rgui);
      assert(before == copied && !rgui.upscale_buf.data);
      assert(!(display.flags & GFX_DISP_FLAG_FB_DIRTY));
      taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr->texture.frames);
      width = level == RGUI_UPSCALE_AUTO ? 12 : level * 4;
      assert(taken && taken->dims == VIDEO_SCALE_PACK(width, width));
      for (y = 0; y < width; y++)
         for (x = 0; x < width; x++)
         {
            unsigned sx = (x * ((4 << 16) / width)) >> 16;
            unsigned sy = (y * ((4 << 16) / width)) >> 16;
            assert(((uint16_t*)taken->frame)[y * width + x] == pixels[sy * 4 + sx]);
         }
      rgui_set_texture(&rgui);
      assert(!retro_triple_buffer_take(&thr->texture.frames));
   }
   active = false;
   settings.uints.menu_rgui_internal_upscale_level = 2;
   display.flags = GFX_DISP_FLAG_FB_DIRTY;
   before = copied;
   rgui_set_texture(&rgui);
   assert(copied == before + 1 && rgui.upscale_buf.data);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr->texture.frames);
   assert(taken && taken->dims == VIDEO_SCALE_PACK(8, 8));
   assert(!memcmp(taken->frame, rgui.upscale_buf.data, 8 * 8 * sizeof(uint16_t)));
   free(rgui.upscale_buf.data);
   rgui.upscale_buf.data = NULL;
   poke.get_menu_framebuffer = direct_get;
   poke.set_texture_frame = direct_submit;
   display.flags = GFX_DISP_FLAG_FB_DIRTY;
   before = copied;
   rgui_set_texture(&rgui);
   assert(direct_submits == 1 && copied == before && !rgui.upscale_buf.data);
   poke.get_menu_framebuffer = NULL;
   poke.set_texture_frame = thread_set_texture_frame;
   active = true;
   viewport_dims = VIDEO_SCALE_PACK(2, 2);
   display.flags = GFX_DISP_FLAG_FB_DIRTY;
   rgui_set_texture(&rgui);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr->texture.frames);
   assert(taken && taken->dims == VIDEO_SCALE_PACK(4, 4));
   assert(!memcmp(taken->frame, pixels, sizeof(pixels)));
}

int main(void)
{
   thread_video_t thr;
   struct video_thread_menu_texture *held, *taken;
   uint32_t *loan;
   uint16_t copied_frame[16];
   unsigned i, gen, before;
   memset(&thr, 0, sizeof(thr));
   retro_triple_buffer_init(&thr.texture.frames, &thr.texture.slot[0],
         &thr.texture.slot[1], &thr.texture.slot[2]);
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false));
   video_thread_thr_capture = &thr;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false));
   active = true;
   assert(!video_thread_get_menu_framebuffer(0, false));
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(0, 4), false));
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 0), false));
   assert(!allocated);

   fail_alloc = true;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false));
   assert(!retro_triple_buffer_take(&thr.texture.frames));
   fail_alloc = false;
   loan = (uint32_t*)video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), true);
   assert(loan);
   for (i = 0; i < 16; i++) loan[i] = 0x12345678;
   before = allocated;
   thread_set_texture_frame(&thr, loan, true, VIDEO_SCALE_PACK(4, 4), 0.5f);
   assert(!copied && before == allocated);
   held = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr.texture.frames);
   assert(held && held->frame == loan && held->rgb32 && held->alpha == 0.5f);
   assert(held->dims == VIDEO_SCALE_PACK(4, 4));

   /* The consumer keeps this image while publications replace each other. */
   for (gen = 1; gen <= 1000; gen++)
   {
      uint16_t *pixels = (uint16_t*)video_thread_get_menu_framebuffer(
            VIDEO_SCALE_PACK(8, 8), false);
      assert(pixels && pixels != held->frame);
      for (i = 0; i < 64; i++) pixels[i] = (uint16_t)gen;
      thread_set_texture_frame(&thr, pixels, false, VIDEO_SCALE_PACK(8, 8), 1.0f);
      for (i = 0; i < 16; i++) assert(((uint32_t*)held->frame)[i] == 0x12345678);
   }
   assert(!copied);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr.texture.frames);
   assert(taken && !taken->rgb32 && taken->dims == VIDEO_SCALE_PACK(8, 8));
   for (i = 0; i < 64; i++) assert(((uint16_t*)taken->frame)[i] == 1000);
   assert(!retro_triple_buffer_take(&thr.texture.frames));

   /* An abandoned loan stays unpublished, and growth failure preserves it. */
   loan = (uint32_t*)video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), true);
   assert(loan);
   fail_alloc = true;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(128, 128), true));
   assert(!retro_triple_buffer_take(&thr.texture.frames));
   fail_alloc = false;
   assert(video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), true) == loan);
   for (i = 0; i < 16; i++) copied_frame[i] = (uint16_t)(i + 42);
   thread_set_texture_frame(&thr, copied_frame, false, VIDEO_SCALE_PACK(4, 4), 0.25f);
   assert(copied == 1);
   taken = (struct video_thread_menu_texture*)retro_triple_buffer_take(&thr.texture.frames);
   assert(taken && taken->frame != copied_frame && taken->alpha == 0.25f);
   assert(!memcmp(taken->frame, copied_frame, sizeof(copied_frame)));
   thread_set_texture_frame(&thr, NULL, false, VIDEO_SCALE_PACK(4, 4), 1.0f);
   assert(!retro_triple_buffer_take(&thr.texture.frames));
   check_upscale(&thr);
   active = false;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false));
   video_thread_thr_capture = NULL;
   assert(!video_thread_get_menu_framebuffer(VIDEO_SCALE_PACK(4, 4), false));
   for (i = 0; i < 3; i++) free(thr.texture.slot[i].frame);
   puts("PASS: menu loans copy no pixels; ownership, replacement, formats and fallback");
   return 0;
}
'''
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-menu-framebuffer-") as directory:
    directory = Path(directory)
    src = directory / "menu.c"
    exe = directory / ("menu.exe" if os.name == "nt" else "menu")
    src.write_text(fixture.replace("STRUCTURE", structure).replace("FUNCTIONS", functions).replace("FRONTEND", frontend).replace("RENDER", render), encoding="utf-8")
    subprocess.run(cc + flags + ["-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                   "-Werror=declaration-after-statement", "-I" + str(root),
                   "-I" + str(root / "libretro-common/include"), str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
