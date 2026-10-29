#!/usr/bin/env python3
"""Exercise the production GLES3 PBO initializer with scripted GL calls."""
import os
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, default=root / "gfx/drivers/gl2.c")
source = parser.parse_args().source.read_text()
start = source.index("static bool gl2_init_pbo_readback(gl2_t *gl)\n{")
end = source.index("\nstatic const gfx_ctx_driver_t *gl2_get_context", start)
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define HAVE_OPENGLES3
#define VIDEO_SCALE_W(d) ((d) >> 16)
#define VIDEO_SCALE_H(d) ((d) & 65535)
#define VIDEO_SCALE_AREA(d) (VIDEO_SCALE_W(d) * VIDEO_SCALE_H(d))
typedef struct { int in_width, in_height; } scaler_t;
typedef struct {
   struct { unsigned dims; } vp;
   scaler_t pbo_readback_scaler;
   unsigned pbo_readback_index, pbo_readback[4];
   unsigned char pbo_readback_valid[4];
} gl2_t;
static unsigned allocations;
static void gl2_pbo_readback_free_syncs(gl2_t *gl) { (void)gl; }
static void glGenBuffers(int n, unsigned *ids)
{ int i; for (i = 0; i < n; i++) ids[i] = (unsigned)i + 1; }
static void gl2_renderchain_bind_pbo(unsigned id) { (void)id; }
static void gl2_renderchain_unbind_pbo(void) {}
static void gl2_renderchain_init_pbo(unsigned size, const void *data)
{ assert(size); assert(!data); allocations++; }
'''
unit += source[start:end]
unit += r'''
int main(void)
{
   gl2_t gl;
   unsigned i;
   memset(&gl, 0, sizeof(gl));
   gl.vp.dims = (64u << 16) | 32u;
   assert(gl2_init_pbo_readback(&gl));
   assert(gl.pbo_readback_scaler.in_width == 64);
   assert(gl.pbo_readback_scaler.in_height == 32);
   for (i = 0; i < 4; i++)
   {
      if ((unsigned)gl.pbo_readback_scaler.in_width != VIDEO_SCALE_W(gl.vp.dims)
            || (unsigned)gl.pbo_readback_scaler.in_height != VIDEO_SCALE_H(gl.vp.dims))
         assert(gl2_init_pbo_readback(&gl));
      gl.pbo_readback_valid[i] = 1;
   }
   assert(allocations == 4);
   for (i = 0; i < 4; i++) assert(gl.pbo_readback_valid[i]);
   gl.vp.dims = (32u << 16) | 16u;
   assert(gl2_init_pbo_readback(&gl));
   assert(gl.pbo_readback_scaler.in_width == 32);
   assert(gl.pbo_readback_scaler.in_height == 16);
   for (i = 0; i < 4; i++) assert(!gl.pbo_readback_valid[i]);
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="gles3-pbo-") as directory:
    path = Path(directory)
    cfile = path / "test.c"
    exe = path / "test.exe"
    cfile.write_text(unit)
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                   ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
                    str(cfile), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
print("PASS GLES3 PBO dimensions, ring retention and resize")
