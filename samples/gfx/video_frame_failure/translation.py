#!/usr/bin/env python3
"""Check production translation ownership transfer and snapshot bounds."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
s = (root / 'tasks/task_translation.c').read_text()
start = s.index('struct translation_sw_ctx\n')
end = s.index('\nbool run_translation_service(', start)
callback = s[start:end]
start = s.index('         if (vp.dims == dims)')
end = s.index('\n      }\n      else', start)
transfer = s[start:end]
program = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gfx/video_frame.h>
#include "gfx/video_defines.h"
''' + callback + r'''
static bool transfer_test(uint8_t **out, uint8_t **prev, unsigned dims)
{
   struct { unsigned dims; } vp;
   struct scaler_ctx context;
   struct scaler_ctx *scaler = &context;
   uint8_t *bit24_image = NULL;
   uint8_t *bit24_image_prev = *prev;
   bool result = true;
   memset(&context, 0, sizeof(context));
   vp.dims = VIDEO_SCALE_PACK(4, 4);
''' + transfer + r'''
   goto done;
finish:
   result = false;
done:
   *out = bit24_image;
   *prev = bit24_image_prev;
   scaler_ctx_gen_reset(scaler);
   return result;
}
int main(void)
{
   struct scaler_ctx scaler;
   struct translation_sw_ctx ctx;
   uint32_t pixels[16];
   uint8_t output[50];
   uint8_t *owned, *out, *prev;
   unsigned i;
   memset(&scaler, 0, sizeof(scaler));
   scaler.in_fmt = SCALER_FMT_ARGB8888;
   ctx.scaler = &scaler;
   ctx.dst = output + 1;
   ctx.dims = VIDEO_SCALE_PACK(4, 4);
   ctx.success = false;
   memset(output, 0xa5, sizeof(output));
   memset(pixels, 0, sizeof(pixels));
   translation_sw_convert_cb(&ctx, pixels, VIDEO_SCALE_PACK(8, 8), 32);
   assert(!ctx.success);
   for (i = 0; i < sizeof(output); i++) assert(output[i] == 0xa5);
   translation_sw_convert_cb(&ctx, pixels, ctx.dims, 16);
   assert(ctx.success && output[0] == 0xa5 && output[49] == 0xa5);
   scaler_ctx_gen_reset(&scaler);
   owned = (uint8_t*)calloc(48, 1);
   assert(owned);
   prev = owned;
   assert(transfer_test(&out, &prev, VIDEO_SCALE_PACK(4, 4)));
   assert(out == owned && prev == NULL);
   free(out);
   prev = (uint8_t*)calloc(48, 1);
   assert(prev);
   assert(transfer_test(&out, &prev, VIDEO_SCALE_PACK(2, 2)));
   assert(out && out != prev && prev);
   free(out);
   free(prev);
   puts("PASS: translation snapshot bounds and zero-copy readback ownership transfer");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='translation-frame-') as tmp:
    tmp = Path(tmp)
    src = tmp / 'test.c'
    src.write_text(program)
    exe = tmp / 'translation_test.exe'
    lrc = root / 'libretro-common'
    cmd = shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=gnu89', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
        '-Wdeclaration-after-statement', '-I'+str(root), '-I'+str(lrc/'include'),
        str(src)]
    cmd += [str(lrc/'gfx/scaler'/f) for f in ('scaler.c','scaler_int.c','pixconv.c','scaler_filter.c')]
    cmd += ['-lm', '-o', str(exe)]
    subprocess.run(cmd, check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
