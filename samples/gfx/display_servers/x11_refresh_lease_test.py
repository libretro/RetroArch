"""Exercise the actual X11 refresh lease without an X server or X11 headers."""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--cc", default=os.environ.get("CC", "cc"))
p.add_argument("--runner", default="")
p.add_argument("--sanitizer", default="")
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
source = (root / "gfx/display_servers/dispserv_x11.c").read_text(encoding="utf-8")

def function(name):
    end = source.index("\n}", source.index(name + "(")) + 2
    start = source.rfind("\nstatic ", 0, source.index(name + "(")) + 1
    return source[start:end]

start = source.index("typedef struct\n{\n   RROutput *outputs;")
end = source.index("#endif /* HAVE_XRANDR */", start)
unit = r"""
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int bool;
#define true 1
#define false 0
#define VIDEO_SCALE_PACK(w,h) (((w) << 16) | (h))
#define VIDEO_SCALE_W(d) ((d) >> 16)
#define VIDEO_SCALE_H(d) ((d) & 65535)
typedef unsigned long RROutput;
typedef unsigned long RRCrtc;
typedef unsigned long RRMode;
typedef unsigned long Window;
typedef unsigned short Rotation;
typedef int Status;
typedef int Bool;
typedef struct { int unused; } xError;
typedef struct { int id; } Display;
typedef struct { int error_code, request_code, minor_code; } XErrorEvent;
typedef struct { RRMode id; unsigned width, height, hTotal, vTotal;
   unsigned long dotClock, modeFlags; } XRRModeInfo;
typedef struct { int nmode, noutput; XRRModeInfo *modes; } XRRScreenResources;
typedef struct { RRCrtc crtc; int connection, nmode;
   RRMode *modes; char *name; } XRROutputInfo;
typedef struct { RRMode mode; int x, y, noutput;
   Rotation rotation; RROutput *outputs; } XRRCrtcInfo;
typedef struct { int id; } dispserv_x11_t;
#define BadValue 2
#define BadMatch 8
#define BadAlloc 11
#define BadAccess 10
#define RRNumberErrors 4
#define DisplayString(d) "mock"
#define RR_Connected 0
#define RR_Interlace 16
#define RR_DoubleScan 32
#define X_RRSetCrtcConfig 21
#define RRSetConfigSuccess 0
#define CurrentTime 0
#define RootWindow(d,s) ((Window)1)
#define DefaultScreen(d) 0
static Display display, foreign_display, control_display;
static XRRModeInfo modes[5];
static RRMode advertised[5];
static RROutput outputs[2] = { 10, 11 }, other_output = 12;
static XRRCrtcInfo heads[2];
static XRRScreenResources resources;
static XRROutputInfo output_info[3];
static int selected_head, calls, invalidations, forwarded;
static int unavailable, disconnected, reject, protocol_error, xwayland;
static int allocation_failure, allocations;
static int control_connections, control_fail, extension_fail;
static Bool (*wire_handler)(Display *, XErrorEvent *, xError *);
static int previous(Display *d, XErrorEvent *e)
{ (void)d; (void)e; forwarded++; return 0; }
static void *test_malloc(size_t n)
{
   void *p;
   if (allocation_failure && --allocation_failure == 0) return NULL;
   p = malloc(n); if (p) allocations++; return p;
}
static void *test_calloc(size_t n, size_t size)
{
   void *p = test_malloc(n * size);
   if (p) memset(p, 0, n * size);
   return p;
}
static void test_free(void *p) { if (p) allocations--; free(p); }
static Display *x11_display_server_open_display(dispserv_x11_t *s)
{ return s && !unavailable ? &display : NULL; }
static void x11_display_server_close_display(dispserv_x11_t *s, Display *d)
{ (void)s; (void)d; }
static XRRScreenResources *XRRGetScreenResourcesCurrent(Display *d, Window w)
{ (void)d; (void)w; return unavailable ? NULL : &resources; }
static void XRRFreeScreenResources(XRRScreenResources *r) { (void)r; }
static XRROutputInfo *XRRGetOutputInfo(Display *d, XRRScreenResources *r, RROutput o)
{ (void)d; (void)r; return o >= 10 && o <= 12 && !disconnected ? &output_info[o-10] : NULL; }
static void XRRFreeOutputInfo(XRROutputInfo *o) { (void)o; }
static XRRCrtcInfo *XRRGetCrtcInfo(Display *d, XRRScreenResources *r, RRCrtc c)
{ (void)d; (void)r; return c >= 100 && c <= 101 ? &heads[c-100] : NULL; }
static void XRRFreeCrtcInfo(XRRCrtcInfo *c) { (void)c; }
static bool x11_res_pick_output(Display *d, XRRScreenResources *r, Window w,
      int index, XRROutputInfo **o, XRRCrtcInfo **c)
{
   (void)d; (void)r; (void)w; (void)index;
   if (disconnected) return false;
   *o = &output_info[selected_head ? 2 : 0]; *c = &heads[selected_head];
   return true;
}
static int XQueryExtension(Display *d, const char *name, int *opcode, int *event, int *error)
{
   (void)d; *opcode = 140; *event = *error = 0;
   return !strcmp(name,"RANDR") || (!strcmp(name,"XWAYLAND") && xwayland);
}
static Display *XOpenDisplay(const char *name)
{ (void)name; if (control_fail) return NULL; control_connections++; return &control_display; }
static int XCloseDisplay(Display *d)
{ assert(d==&control_display); control_connections--; wire_handler=NULL; return 0; }
static int XRRQueryExtension(Display *d, int *event, int *error)
{ (void)d; *event=90; *error=150; return !extension_fail; }
static Bool (*XESetWireToError(Display *d, int error,
      Bool (*h)(Display *, XErrorEvent *, xError *)))(Display *, XErrorEvent *, xError *)
{
   (void)error; assert(d==&control_display); wire_handler=h; return NULL;
}
static void x11_refresh_invalidate(void) { invalidations++; }
static Status XRRSetCrtcConfig(Display *d, XRRScreenResources *r, RRCrtc c,
      unsigned long time, int x, int y, RRMode mode, Rotation rotation,
      RROutput *out, int count)
{
   XRRCrtcInfo *head = &heads[c-100];
   XErrorEvent e;
   xError wire;
   (void)r; (void)time;
   calls++;
   assert(head->x == x && head->y == y && head->rotation == rotation);
   assert(count == head->noutput && out == head->outputs);
   if (protocol_error)
   {
      memset(&e,0,sizeof(e)); e.error_code=1; e.request_code=140;
      e.minor_code=X_RRSetCrtcConfig;
      previous(&foreign_display,&e);
      e.minor_code=0;
      assert(wire_handler(d,&e,&wire)); previous(d,&e);
      e.minor_code=X_RRSetCrtcConfig;
      assert(!wire_handler(d,&e,&wire));
      return 3;
   }
   if (reject) return 3;
   head->mode = mode;
   return RRSetConfigSuccess;
}
#define malloc test_malloc
#define calloc test_calloc
#define free test_free
"""
header = (root / "gfx/video_display_server.h").read_text(encoding="utf-8")
config_start = header.index("typedef struct video_display_config")
config_end = header.index("} video_display_config_t;", config_start) + len("} video_display_config_t;")
unit += header[config_start:config_end]
policy = (root / "gfx/gfx_media_display.c").read_text(encoding="utf-8")
unit += policy[policy.index("float gfx_media_display_rate("):policy.index("void gfx_media_display_owner_reset(")]
unit += "\n" + function("x11_res_find_mode") + "\n" + function("x11_res_mode_rate")
unit += "\n" + function("x11_res_is_xwayland") + "\n" + source[start:end]
unit += r"""
#define HAVE_X11 1
#define HAVE_XRANDR 1
typedef struct { int id; } video_display_server_t;
static int video_media_refresh_owner, video_media_refresh_output_changed;
static int video_media_x11_serial, g_x11_refresh_serial;
static unsigned owner_updates, owner_resets;
static bool owner_changed;
static struct { bool window_refresh_known; } video_driver_st;
static int test_exchange(int *p, int value)
{ int old = *p; *p = value; return old; }
#define retro_atomic_exchange_int test_exchange
#define retro_atomic_load_acquire_int(p) (*(p))
static const video_display_server_t *video_display_server_modes(void **data)
{
   static video_display_server_t server;
   *data = &server;
   return &server;
}
static void gfx_media_display_owner_reset(int *owner)
{ (void)owner; owner_resets++; }
static void gfx_media_display_owner_update(int *owner,
      const video_display_server_t *server, void *data,
      unsigned num, unsigned den, bool changed)
{
   (void)owner; (void)server; (void)data; (void)num; (void)den;
   owner_updates++; owner_changed = changed;
}
"""
driver = (root / "gfx/video_driver.c").read_text(encoding="utf-8")
driver_start = driver.index("void video_display_server_media_refresh(")
driver_end = driver.index("\n}", driver_start) + 2
unit += driver[driver_start:driver_end]
unit += r"""
static void test_notifications(void)
{
   video_driver_st.window_refresh_known = true;
   video_display_server_media_refresh(24,1);
   assert(owner_updates==1 && !owner_changed);
   g_x11_refresh_serial++;
   video_display_server_media_refresh(24,1);
   assert(owner_updates==2 && owner_changed && !video_driver_st.window_refresh_known);
   video_driver_st.window_refresh_known = true;
   video_display_server_media_refresh(24,1);
   assert(owner_updates==3 && !owner_changed && video_driver_st.window_refresh_known);
   video_media_refresh_output_changed = 1;
   video_display_server_media_refresh(24,1);
   assert(owner_updates==4 && owner_changed && !video_media_refresh_output_changed);
   video_display_server_media_refresh(0,0);
   assert(owner_updates==4 && owner_resets==1);
}
"""
unit += r"""
static void reset(void)
{
   unsigned i;
   assert(allocations == 0);
   memset(modes,0,sizeof(modes)); memset(heads,0,sizeof(heads));
   for (i=0;i<5;i++)
   {
      modes[i].id=i+1; modes[i].width=1920; modes[i].height=1080;
      modes[i].hTotal=2000; modes[i].vTotal=1100; advertised[i]=i+1;
   }
   modes[0].dotClock=132000000;
   modes[1].dotClock=264000000;
   modes[2].dotClock=105600000; modes[2].width=1280;
   modes[3].dotClock=263736264;
   modes[4].dotClock=132000000; modes[4].modeFlags=RR_Interlace;
   resources.nmode=5; resources.noutput=3; resources.modes=modes;
   outputs[0]=10; outputs[1]=11;
   heads[0].mode=1; heads[0].x=40; heads[0].y=30; heads[0].rotation=2;
   heads[0].noutput=2; heads[0].outputs=outputs;
   heads[1]=heads[0]; heads[1].x=1960;
   heads[1].outputs=&other_output; heads[1].noutput=1;
   for (i=0;i<3;i++)
   { output_info[i].crtc=i==2?101:100; output_info[i].connection=RR_Connected;
     output_info[i].nmode=5; output_info[i].modes=advertised;
     output_info[i].name="DP-1"; }
   selected_head=calls=invalidations=forwarded=0;
   unavailable=disconnected=reject=protocol_error=xwayland=allocation_failure=0;
   assert(!control_connections); control_fail=extension_fail=0;
}
int main(void)
{
   dispserv_x11_t server;
   x11_media_refresh_t *lease;
   test_notifications();
   reset();
   lease=x11_display_server_begin_media_refresh(&server,24,1);
   assert(lease && heads[0].mode==2 && heads[1].mode==1 && calls==1);
   assert(x11_display_server_media_refresh_current(&server,lease,30,1));
   assert(!x11_display_server_media_refresh_current(&server,lease,25,1));
   outputs[0]=11; outputs[1]=10;
   assert(x11_display_server_media_refresh_current(&server,lease,24,1));
   selected_head=1;
   assert(!x11_display_server_media_refresh_current(&server,lease,24,1));
   x11_display_server_end_media_refresh(&server,lease);
   assert(heads[0].mode==1 && heads[1].mode==1 && calls==2 && allocations==0);
   lease=x11_display_server_begin_media_refresh(&server,24,1);
   assert(lease && lease->crtc==101 && heads[0].mode==1 && heads[1].mode==2);
   x11_display_server_end_media_refresh(&server,lease);
   assert(heads[0].mode==1 && heads[1].mode==1);
   reset(); lease=x11_display_server_begin_media_refresh(&server,24000,1001);
   assert(lease && heads[0].mode==4);
   x11_display_server_end_media_refresh(&server,lease);
   assert(heads[0].mode==1);
   reset(); lease=x11_display_server_begin_media_refresh(&server,24,1);
   heads[0].mode=3;
   x11_display_server_end_media_refresh(&server,lease);
   assert(heads[0].mode==3 && calls==1);
   reset(); lease=x11_display_server_begin_media_refresh(&server,24,1);
   heads[0].x++;
   x11_display_server_end_media_refresh(&server,lease);
   assert(heads[0].mode==2 && calls==1);
   reset(); lease=x11_display_server_begin_media_refresh(&server,24,1);
   disconnected=1;
   x11_display_server_end_media_refresh(&server,lease);
   assert(calls==1 && allocations==0);
   reset(); modes[2].width=1920;
   lease=x11_display_server_begin_media_refresh(&server,24,1);
   assert(lease && heads[0].mode==3);
   x11_display_server_end_media_refresh(&server,lease);
   reset(); output_info[1].nmode=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls);
   reset(); xwayland=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls);
   reset(); output_info[0].name="XWAYLAND0";
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls);
   reset(); control_fail=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls && !allocations);
   reset(); extension_fail=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls && !allocations);
   reset(); reject=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && allocations==0);
   reset(); protocol_error=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1));
   assert(forwarded==2 && !control_connections && allocations==0);
   reset(); allocation_failure=1;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls);
   reset(); allocation_failure=2;
   assert(!x11_display_server_begin_media_refresh(&server,24,1) && !calls && allocations==0);
   reset();
   assert(!x11_display_server_begin_media_refresh(&server,30,1) && !calls);
   assert(!x11_display_server_begin_media_refresh(&server,0,1));
   assert(!x11_display_server_begin_media_refresh(NULL,24,1));
   x11_display_server_end_media_refresh(&server,NULL);
   assert(allocations==0);
   puts("PASS: X11 fractional refresh, clone intersection, output ownership and conditional restore");
   return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="x11-refresh-lease-") as temp:
    src = Path(temp) / "lease.c"
    src.write_text(unit, encoding="utf-8")
    exe = Path(temp) / ("lease.exe" if os.name == "nt" or a.runner else "lease")
    flags = ["-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-parameter", "-Wframe-larger-than=2048", "-I" + str(root),
             "-I" + str(root / "libretro-common/include")]
    if a.sanitizer:
        flags += ["-fsanitize=" + a.sanitizer, "-fno-omit-frame-pointer"]
    subprocess.run(shlex.split(a.cc) + flags + [str(src),
        "-o", str(exe), "-lm"], check=True)
    subprocess.run(shlex.split(a.runner) + [str(exe)], check=True)
