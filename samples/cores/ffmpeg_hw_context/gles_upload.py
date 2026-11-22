#!/usr/bin/env python3
"""Exercise the shipping FFmpeg GL upload against counted client-memory reads."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'cores/libretro-ffmpeg/ffmpeg_core.c').read_text()
start = source.index('#ifdef HAVE_OPENGLES\n               data ')
end = source.index('video_buffer_open_slot(VIDEO_BUFFER_STR, ctx);', start)
body = source[start:end] + 'video_buffer_open_slot(VIDEO_BUFFER_STR, ctx);'

fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define GL_PIXEL_UNPACK_BUFFER 1
#define GL_WRITE_ONLY 2
#define GL_MAP_WRITE_BIT 4
#define GL_MAP_INVALIDATE_BUFFER_BIT 8
#define GL_TEXTURE_2D 16
#define GL_RGBA 32
#define GL_BGRA 64
#define GL_UNSIGNED_BYTE 128
#define GL_UNSIGNED_INT_8_8_8_8_REV 256
typedef unsigned GLuint;
static struct { unsigned width, height; } media;
static struct { unsigned pbo, tex; } frames[2];
typedef struct { uint8_t *data[1]; int linesize[1]; } frame;
typedef struct { frame *target; } video_decoder_context_t;
static uint32_t scratch[128], pbo[128];
static uint8_t pixels[512], expected[512], uploaded[512];
static unsigned copied, uploads, releases, mapped, bound;
static const void *upload_pointer;
#define MEDIA_STR media
#define FRAMES_STR frames
#define VIDEO_FRAME_TEMP_BUFFER_STR scratch
#define VIDEO_BUFFER_STR NULL
static void glBindBuffer(unsigned target, unsigned buffer)
{ assert(target == GL_PIXEL_UNPACK_BUFFER); bound = buffer; }
static void *glMapBufferRange(unsigned target, unsigned offset, unsigned size, unsigned flags)
{ assert(target == GL_PIXEL_UNPACK_BUFFER && bound && !offset && size <= sizeof(pbo));
  (void)flags; mapped = 1; return pbo; }
static void *glMapBuffer(unsigned target, unsigned access)
{ (void)access; return glMapBufferRange(target, 0, sizeof(pbo), 0); }
static void glUnmapBuffer(unsigned target)
{ assert(target == GL_PIXEL_UNPACK_BUFFER && mapped); mapped = 0; }
static void glBindTexture(unsigned target, unsigned texture)
{ assert(target == GL_TEXTURE_2D); (void)texture; }
static void glTexImage2D(unsigned target, int level, int internal,
      unsigned width, unsigned height, int border, unsigned format,
      unsigned type, const void *data)
{
   assert(target == GL_TEXTURE_2D && !level && internal == GL_RGBA && !border);
   assert(width == media.width && height == media.height && !mapped && !releases);
#ifdef HAVE_OPENGLES
   assert(format == GL_RGBA && type == GL_UNSIGNED_BYTE && data && !bound);
#else
   assert(format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8_REV && !data && bound);
#endif
   upload_pointer = data;
   memcpy(uploaded, data ? data : pbo, width * height * 4);
   uploads++;
}
static void video_buffer_open_slot(void *buffer, video_decoder_context_t *ctx)
{ (void)buffer; assert(ctx && uploads == 1 && !mapped && !bound); releases++; }
static void *counted_copy(void *dst, const void *src, size_t size)
{ assert(!uploads && !releases); copied += (unsigned)size; return memcpy(dst, src, size); }
#define memcpy counted_copy
static void upload(video_decoder_context_t *ctx)
{
   unsigned y;
   int stride, width;
   const uint8_t *src = NULL;
   uint32_t *data = NULL;
   /* SHIPPING_UPLOAD */
}
#undef memcpy
int main(void)
{
   static const unsigned widths[] = {1, 3, 8, 16};
   unsigned w, padding, height, y, x;
   frame target;
   video_decoder_context_t ctx;
   ctx.target = &target;
   frames[1].pbo = frames[1].tex = 1;
   for (w = 0; w < sizeof(widths) / sizeof(widths[0]); w++)
      for (padding = 0; padding <= 16; padding += 4)
         for (height = 1; height <= 4; height++)
         {
            unsigned row = widths[w] * 4;
            unsigned bytes = row * height;
            media.width = widths[w]; media.height = height;
            target.linesize[0] = (int)(row + padding);
            target.data[0] = pixels;
            memset(pixels, 0xed, sizeof(pixels));
            memset(scratch, 0xcd, sizeof(scratch));
            memset(pbo, 0xcd, sizeof(pbo));
            for (y = 0; y < height; y++)
               for (x = 0; x < row; x++)
                  expected[y * row + x] = pixels[y * (row + padding) + x]
                     = (uint8_t)(x + y * 37 + w);
            copied = uploads = releases = mapped = bound = 0;
            upload(&ctx);
            assert(uploads == 1 && releases == 1 && !memcmp(expected, uploaded, bytes));
#ifdef HAVE_OPENGLES
            assert(copied == (padding ? bytes : 0));
            assert(upload_pointer == (padding ? (const void*)scratch : (const void*)pixels));
            if (!padding) assert(((uint8_t*)scratch)[0] == 0xcd);
#else
            assert(copied == bytes);
#endif
            for (y = 0; y < height; y++)
               for (x = row; x < row + padding; x++)
                  assert(pixels[y * (row + padding) + x] == 0xed);
         }
   puts("PASS FFmpeg upload: packed borrows, padded rows, pixels and slot lifetime");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ffmpeg-upload-') as directory:
    unit = Path(directory) / 'upload.c'
    unit.write_text(fixture.replace('/* SHIPPING_UPLOAD */', body))
    for defines in (['-DHAVE_OPENGLES'], [], ['-D__MACH__']):
        exe = Path(directory) / 'upload.exe'
        subprocess.run(shlex.split(os.environ.get('CC', 'cc')) +
                       ['-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                        '-Wno-unused-function', '-Wframe-larger-than=4096'] +
                       shlex.split(os.environ.get('CFLAGS', '')) + defines +
                       [str(unit), '-o', str(exe)], check=True)
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
