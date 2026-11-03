#!/usr/bin/env python3
"""Test that WGL keeps a cached core-profile context instead of replacing it.

create_gl_context() acknowledges a kept context, so the core is not
reset, but a core-profile or debug context (glcore always, gl when the
core asks for 3.1 or later) was then made again over it: the core went
on with objects the new context does not have, and the old shared
context leaked. The real function runs here against stubbed WGL calls.
"""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers_context/wgl_ctx.c").read_text()

match = re.search(r"^void create_gl_context\(HWND hwnd, bool \*quit\)\n\{", source, re.M)
assert match, "create_gl_context"
pos = source.index("{", match.start())
depth, end = 1, pos + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
function = source[match.start():end]

fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
#define APIENTRY
#define WINAPI
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define RARCH_LOG(...) ((void)0)
#define RARCH_ERR(...) ((void)0)
#define WGL_CONTEXT_MAJOR_VERSION_ARB    0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB    0x2092
#define WGL_CONTEXT_FLAGS_ARB            0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB     0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB        0x0001
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001
#define VIDEO_FLAG_HDR_SUPPORT   1
#define VIDEO_FLAG_HDR10_SUPPORT 2
#define VIDEO_FLAG_SCRGB_SUPPORT 4
#define WIN32_CMN_FLAG_INITED    1
enum { WGL_FLAG_USE_HW_CTX = 1, WGL_FLAG_ADAPTIVE_VSYNC = 2 };
typedef int *HWND;
typedef int *HDC;
typedef int *HGLRC;
typedef void (*gfx_ctx_proc_t)(void);
struct retro_hw_render_callback { bool debug_context; };

static struct retro_hw_render_callback hwr;
static int contexts[16], made, deleted, acks, shared;
static HGLRC current;
static int dc;
static HDC win32_hdc;
static HGLRC win32_hrc, win32_hw_hrc;
static unsigned win32_major, win32_minor;
static unsigned wgl_flags, g_win32_flags;

static struct retro_hw_render_callback *video_driver_get_hw_context(void) { return &hwr; }
static HDC GetDC(HWND w) { (void)w; return &dc; }
static void win32_setup_pixel_format(HDC h, bool b) { (void)h; (void)b; }
static void video_driver_cache_context_ack_set(void) { acks++; }
static HGLRC wglCreateContext(HDC h) { (void)h; return &contexts[made++]; }
static bool wglShareLists(HGLRC a, HGLRC b) { (void)a; (void)b; shared++; return true; }
static bool wglMakeCurrent(HDC h, HGLRC c) { (void)h; current = c; return true; }
static bool wglDeleteContext(HGLRC c) { (void)c; deleted++; return true; }
static HGLRC create_attribs(HDC h, HGLRC share, const int *a)
{ (void)h; (void)share; (void)a; return &contexts[made++]; }
static gfx_ctx_proc_t gfx_ctx_wgl_get_proc_address(const char *s)
{
   if (!strcmp(s, "wglCreateContextAttribsARB"))
      return (gfx_ctx_proc_t)create_attribs;
   return NULL;
}
static bool wgl_has_extension(const char *e, const char *x) { (void)e; (void)x; return false; }
static void video_driver_modify_disp_flags(unsigned s, unsigned c) { (void)s; (void)c; }
static HWND win32_get_window(void) { return NULL; }
static bool win32_display_hdr_active(HWND w) { (void)w; return false; }

FUNCTION

static void session(unsigned major, unsigned minor, const char *what)
{
   bool quit = false;
   HGLRC hrc, hw;
   made = deleted = acks = shared = 0;
   win32_hrc = win32_hw_hrc = NULL;
   win32_major = major; win32_minor = minor;
   wgl_flags = WGL_FLAG_USE_HW_CTX;

   create_gl_context(NULL, &quit);
   assert(!quit && win32_hrc && win32_hw_hrc && !acks);
   hrc = win32_hrc; hw = win32_hw_hrc;

   /* a restart that keeps the contexts: the same two, no new ones */
   made = deleted = 0;
   create_gl_context(NULL, &quit);
   assert(!quit && acks == 1);
   if (win32_hrc != hrc || win32_hw_hrc != hw || made || deleted)
   {
      printf("FAIL %s: the kept context was replaced (%d made, %d deleted)\n",
            what, made, deleted);
      fflush(stdout);
      assert(0);
   }
   assert(current == hrc);
}

int main(void)
{
   session(2, 0, "compatibility");
   session(3, 2, "core profile");
   hwr.debug_context = true;
   session(2, 0, "debug");
   puts("PASS a kept WGL context is kept, core profile and debug included");
   return 0;
}
'''
fixture = fixture.replace("FUNCTION", function)

with tempfile.TemporaryDirectory(prefix="wgl-context-keep-") as temp:
    unit = Path(temp) / "test.c"
    compiler = shlex.split(os.environ.get("CC", "cc"))
    suffix = ".exe" if os.name == "nt" or "mingw" in compiler[0] else ""
    exe = Path(temp) / ("test" + suffix)
    unit.write_text(fixture)
    subprocess.run(compiler + ["-std=c89", "-pedantic", "-Wall", "-Wextra",
                   "-Werror", "-Wno-unused-function", "-Wno-variadic-macros",
                   "-Wframe-larger-than=2048"]
                   + shlex.split(os.environ.get("CFLAGS", ""))
                   + [str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
