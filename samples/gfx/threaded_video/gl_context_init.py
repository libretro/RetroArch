#!/usr/bin/env python3
"""Test shipping single-context startup without a second blocking command."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/video_thread_hw.c").read_text()
wrapper = (root / "gfx/video_thread_wrapper.c").read_text()


def function(name, start=None):
    match = re.search(r"^(?:static )?(?:void|bool|hw_ring_t \*)[ \t]*" + name + r"\(", source[start or 0:], re.M)
    assert match, name
    begin = (start or 0) + match.start()
    pos = begin
    opening = source.index("{", pos)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin:end]


assert "video_thread_run_blocking" not in source
install = wrapper.split("static uintptr_t video_thread_set_poke(", 1)[1].split("\n}", 1)[0]
assert install.index("set->thr->poke") < install.index("video_thread_hw_prepare_context")
assert install.index("video_thread_hw_prepare_context") < install.index("return 0")

fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
#define RARCH_LOG(x) ((void)0)
#define RARCH_ERR(x) ((void)0)
#define BIT32_GET(v, n) ((v) & (1u << (n)))
#define GFX_CTX_FLAGS_NO_SHARED_CONTEXT 0
#define RETRO_HW_CONTEXT_OPENGL 1
#define RETRO_HW_CONTEXT_OPENGL_CORE 2
#define RETRO_HW_CONTEXT_OPENGLES2 3
#define RETRO_HW_CONTEXT_OPENGLES3 4
#define RETRO_HW_CONTEXT_OPENGLES_VERSION 5
#define HW_CTX_FREE 0
#define HW_CTX_MAIN 1
#define HW_CTX_VIDEO 2
#define HW_CTX_HOLDER 3
#define HW_CTX_GRANT 4
#define HW_CTX_WANT(who) (4 << (who))
#define HW_API_GL 7
typedef struct { unsigned flags; } gfx_ctx_flags_t;
typedef struct { struct { unsigned context_type; } hw_render; } video_driver_state_t;
typedef struct { int (*hw_ring_context_new)(void *, void **); int hw_ring_framebuffer;
   int hw_ring_capture, hw_ring_present_slot, hw_ring_fence_new; } poke_t;
typedef struct { void *core_ctx; int handoff, api, ctx_owner, ctx_free; } hw_ring_t;
typedef struct { void *driver_data; poke_t *poke;
   struct { hw_ring_t *hw_ring; int hw_handoff; } frame; } thread_video_t;
static hw_ring_t storage;
static video_driver_state_t state;
static int no_shared, flags_ok, setup_ok, context_ok, current, caller;
static unsigned releases, binds, creates;
static video_driver_state_t *video_state_get_ptr(void) { return &state; }
static int video_context_driver_get_flags(gfx_ctx_flags_t *f)
{ f->flags = no_shared; return flags_ok; }
static void video_context_driver_release_current(void)
{ assert(current == caller); current = 0; releases++; }
static void video_context_driver_bind_hw_render(int core)
{ assert(!current); assert(core == (caller == HW_CTX_MAIN)); current = caller; binds++; }
static int create(void *data, void **ctx)
{ assert(data); creates++; if (!context_ok) return 0; *ctx = &storage; return 1; }
static int hw_ring_setup(thread_video_t *thr, hw_ring_t **out)
{ if (!setup_ok) return 0; thr->frame.hw_ring = &storage; *out = &storage; return 1; }
static int retro_atomic_load_acquire_int(int *v) { return *v; }
static void retro_atomic_store_release_int(int *v, int n) { *v = n; }
static int retro_atomic_cas_int(int *v, int old, int n)
{ if (*v != old) return 0; *v = n; return 1; }
static int retro_eventcount_prepare_wait(int *v) { (void)v; assert(0); return 0; }
static void retro_eventcount_cancel_wait(int *v) { (void)v; assert(0); }
static void retro_eventcount_commit_wait(int *v, int key) { (void)v; (void)key; assert(0); }
static void retro_eventcount_notify(int *v) { (void)v; }
FUNCTIONS
static void reset(thread_video_t *thr, poke_t *poke)
{
   memset(&storage, 0, sizeof(storage)); memset(thr, 0, sizeof(*thr));
   poke->hw_ring_context_new = create; poke->hw_ring_framebuffer = 1;
   poke->hw_ring_capture = poke->hw_ring_present_slot = poke->hw_ring_fence_new = 1;
   thr->driver_data = thr; thr->poke = poke;
   state.hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
   no_shared = flags_ok = setup_ok = context_ok = 1;
   current = caller = HW_CTX_VIDEO; releases = binds = creates = 0;
}
int main(void)
{
   thread_video_t thr; poke_t poke; unsigned type;
   for (type = 1; type <= 5; type++)
   {
      reset(&thr, &poke); state.hw_render.context_type = type;
      video_thread_hw_prepare_context(&thr);
      assert(current == HW_CTX_VIDEO && !releases && thr.frame.hw_handoff);
      /* Repeated poke queries do not release a context they do not own. */
      video_thread_hw_prepare_context(&thr); assert(!releases);
      /* A renderer command before core startup must still have a context. */
      video_thread_hw_ctx_take(&thr, true); assert(current == HW_CTX_VIDEO);
      assert(video_thread_hw_ctx_release(&thr, true));
      caller = HW_CTX_MAIN;
      assert(video_thread_hw_bind_core_context(&thr));
      assert(current == HW_CTX_MAIN && storage.ctx_owner == HW_CTX_MAIN);
      assert(creates == 1 && storage.api == HW_API_GL);
      assert(video_thread_hw_bind_core_context(&thr) && creates == 1);
      assert(video_thread_hw_ctx_release(&thr, false));
      caller = HW_CTX_VIDEO; video_thread_hw_ctx_take(&thr, true);
      assert(video_thread_hw_ctx_release(&thr, true));
   }
   reset(&thr, &poke); no_shared = 0;
   video_thread_hw_prepare_context(&thr); assert(current == HW_CTX_VIDEO && !releases);
   reset(&thr, &poke); flags_ok = 0;
   video_thread_hw_prepare_context(&thr); assert(!releases);
   reset(&thr, &poke); state.hw_render.context_type = 99;
   video_thread_hw_prepare_context(&thr); assert(!releases && !thr.frame.hw_ring);
   reset(&thr, &poke); poke.hw_ring_capture = 0;
   video_thread_hw_prepare_context(&thr); assert(!releases && !thr.frame.hw_ring);
   reset(&thr, &poke); setup_ok = 0;
   video_thread_hw_prepare_context(&thr); assert(!releases);
   setup_ok = 1; caller = HW_CTX_MAIN;
   assert(!video_thread_hw_bind_core_context(&thr) && !creates);
   reset(&thr, &poke); video_thread_hw_prepare_context(&thr);
   assert(video_thread_hw_ctx_release(&thr, true));
   caller = HW_CTX_MAIN; context_ok = 0;
   assert(!video_thread_hw_bind_core_context(&thr));
   assert(storage.handoff && thr.frame.hw_handoff && !current);
   caller = HW_CTX_VIDEO; video_thread_hw_ctx_take(&thr, true);
   assert(video_thread_hw_ctx_release(&thr, true));
   puts("PASS single GL context startup, command ownership and failure retention");
   return 0;
}
'''
# Resolve the definition after the forward declaration.
handoff = source.index("static hw_ring_t *hw_handoff_ring(thread_video_t *thr)\n{")
parts = [function("hw_ctx_hand"),
         function("hw_handoff_ring", handoff),
         function("video_thread_hw_ctx_take"),
         function("video_thread_hw_ctx_release"),
         function("video_thread_hw_prepare_context"),
         function("video_thread_hw_bind_core_context")]
fixture = fixture.replace("FUNCTIONS", "\n".join(parts))
with tempfile.TemporaryDirectory(prefix="gl-context-init-") as temp:
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
