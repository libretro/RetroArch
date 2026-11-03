#!/usr/bin/env python3
"""Test that a kept GL context is not kept for a core that changed context.

A core that sets hw_render.cache_context is not reset across a video
restart when the context driver kept its contexts (WGL, SDL, GLX set the
acknowledgement). The gl driver hands such a core one of two contexts:
the driver's own, or one shared with it - the latter always under the
threaded wrapper. Framebuffer and vertex array objects are not shared
between the two, so a core moved from one to the other by a threaded
video toggle went on drawing with objects that did not exist there: an
access violation inside the GL driver on the core's next draw. The
acknowledgement has to be withdrawn when the core's context changes, so
that it is reset into the new one.
"""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
video = (root / "gfx/video_driver.c").read_text()
gl2 = (root / "gfx/drivers/gl2.c").read_text()


def function(source, name):
    match = re.search(r"^(?:static )?[\w \t*]*?\b" + name + r"\([^;{]*\)\s*\{",
                      source, re.M)
    assert match, "missing " + name
    pos = source.index("{", match.start())
    depth, end = 1, pos + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


# The gl driver reports the context once it is settled: after the context
# driver kept (or made) it, and after a context that cannot share took the
# shared one away again.
get_context = function(gl2, "gl2_get_context")
note = get_context.find("video_driver_hw_context_shared_note(")
assert note != -1, "gl2_get_context() does not report the core's context"
assert get_context.index("video_context_driver_init_first(") < note
assert get_context.index("GFX_CTX_FLAGS_NO_SHARED_CONTEXT") < note
assert note < get_context.rindex("return gfx_ctx;")

# A core whose context is destroyed renders in none: the next core's first
# context is not a move.
free_hw = function(video, "video_driver_free_hw_context")
assert re.search(r"video_hw_context_shared\s*=\s*-1;", free_hw), \
    "video_driver_free_hw_context() does not forget the core's context"
state = re.search(r"^static int video_hw_context_shared\s*=\s*-1;", video, re.M)
assert state, "video_hw_context_shared"

fixture = r'''
#include <assert.h>
#include <stdio.h>
typedef int bool;
#define true 1
#define false 0
typedef int retro_atomic_int_t;
#define RETRO_ATOMIC_INT_INITIALIZER(v) (v)
static void retro_atomic_fetch_or_int(int *v, int n) { *v |= n; }
static void retro_atomic_fetch_and_int(int *v, int n) { *v &= n; }
static int retro_atomic_load_acquire_int(int *v) { return *v; }
static retro_atomic_int_t video_cache_context_ack = 0;
STATE
FUNCTIONS
static unsigned resets;
/* A restart: the context driver keeps the contexts (or makes them), the
 * gl driver reports the context the core gets, and drivers_init() resets
 * the core unless the acknowledgement still stands. */
static void restart(bool kept, bool shared)
{
   video_driver_cache_context_ack_clear();
   if (kept)
      video_driver_cache_context_ack_set();
   video_driver_hw_context_shared_note(shared);
   if (!video_driver_cache_context_ack_test())
      resets++;
   video_driver_cache_context_ack_clear();
}
int main(void)
{
   /* the decision */
   assert(!video_driver_hw_context_moved(-1, false));
   assert(!video_driver_hw_context_moved(-1, true));
   assert(!video_driver_hw_context_moved(0, false));
   assert(!video_driver_hw_context_moved(1, true));
   assert(video_driver_hw_context_moved(0, true));
   assert(video_driver_hw_context_moved(1, false));

   /* content starts unthreaded: a new context, reset */
   restart(false, false);                assert(resets == 1);
   /* a restart that keeps it, still unthreaded: not reset */
   restart(true, false);                 assert(resets == 1);
   /* threaded video on: the core is moved to the shared context */
   restart(true, true);                  assert(resets == 2);
   /* a restart under the threaded wrapper: kept, not reset */
   restart(true, true);                  assert(resets == 2);
   /* threaded video off: back to the driver's context */
   restart(true, false);                 assert(resets == 3);
   /* the contexts were not kept: reset as always */
   restart(false, false);                assert(resets == 4);
   restart(false, true);                 assert(resets == 5);
   /* a core that asked for the shared context keeps it unthreaded */
   restart(true, true);                  assert(resets == 5);

   puts("PASS a kept GL context is kept only for a core in the same context");
   return 0;
}
'''
fixture = fixture.replace("STATE", state.group(0))
fixture = fixture.replace("FUNCTIONS", "\n".join([
    function(video, "video_driver_cache_context_ack_set"),
    function(video, "video_driver_cache_context_ack_test"),
    function(video, "video_driver_cache_context_ack_clear"),
    function(video, "video_driver_hw_context_moved"),
    function(video, "video_driver_hw_context_shared_note")]))

with tempfile.TemporaryDirectory(prefix="gl-context-move-") as temp:
    unit = Path(temp) / "test.c"
    compiler = shlex.split(os.environ.get("CC", "cc"))
    suffix = ".exe" if os.name == "nt" or "mingw" in compiler[0] else ""
    exe = Path(temp) / ("test" + suffix)
    unit.write_text(fixture)
    subprocess.run(compiler + ["-std=c89", "-pedantic", "-Wall", "-Wextra",
                   "-Werror", "-Wframe-larger-than=2048"]
                   + shlex.split(os.environ.get("CFLAGS", ""))
                   + [str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
