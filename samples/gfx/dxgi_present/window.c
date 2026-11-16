/* The output a window is on, found by its monitor among every
 * adapter's outputs and asked whether it flips a fullscreen window:
 * window.inc, cut by run.py. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef int HRESULT;
typedef unsigned UINT;
typedef void *HWND;
typedef void *HMONITOR;
typedef void *HMODULE;
typedef struct { int id; } GUID;
typedef const GUID *REFIID;
#define WINAPI
#define STDMETHODCALLTYPE
#define MONITOR_DEFAULTTONEAREST 2
#define FAILED(value) ((value) < 0)
#define SUCCEEDED(value) ((value) >= 0)
#define DROP(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)
#define BRIDGE_COMPOSITION_FULLSCREEN 1

static const GUID bridge_IDXGIOutput6  = { 6 };
static const GUID bridge_IDXGIFactory1 = { 1 };
typedef HRESULT (WINAPI *create_factory_t)(REFIID, void **);
static HMODULE bridge_dxgi_dll;

typedef struct bridge_output6 bridge_output6_t;
typedef struct
{
   void *before[28];
   HRESULT (*CheckHardwareCompositionSupport)(bridge_output6_t *, UINT *);
} bridge_output6_vtbl_t;
struct bridge_output6 { const bridge_output6_vtbl_t *lpVtbl; };

typedef struct unknown IUnknown;
struct unknown_vtable { void *before[2]; unsigned (*Release)(IUnknown *); };
struct unknown { const struct unknown_vtable *lpVtbl; };

typedef struct { HMONITOR Monitor; } DXGI_OUTPUT_DESC;
typedef struct output IDXGIOutput;
struct output_vtable {
   HRESULT (*QueryInterface)(IDXGIOutput *, const GUID *, void **);
   HRESULT (*GetDesc)(IDXGIOutput *, DXGI_OUTPUT_DESC *);
   unsigned (*Release)(IDXGIOutput *);
};
struct output { const struct output_vtable *lpVtbl; unsigned index; };

typedef struct adapter IDXGIAdapter1;
struct adapter_vtable {
   HRESULT (*EnumOutputs)(IDXGIAdapter1 *, unsigned, IDXGIOutput **);
   unsigned (*Release)(IDXGIAdapter1 *);
};
struct adapter { const struct adapter_vtable *lpVtbl; unsigned index; };

typedef struct factory IDXGIFactory1;
struct factory_vtable {
   HRESULT (*EnumAdapters1)(IDXGIFactory1 *, unsigned, IDXGIAdapter1 **);
   unsigned (*Release)(IDXGIFactory1 *);
};
struct factory { const struct factory_vtable *lpVtbl; };

/* Two adapters of two outputs each; output k of adapter a shows
 * monitor a * 2 + k + 1, and supports[] says what each can do. */
static int  refs, factories, asked_output = -1;
static UINT supports[4];
static int  no_dll, no_entry, no_factory, no_output6;
static HMONITOR window_monitor;

static unsigned release6(IUnknown *p) { (void)p; return --refs; }
static HRESULT check(bridge_output6_t *p, UINT *flags)
{ (void)p; *flags = supports[asked_output]; return 0; }
static struct { void *lpVtbl; } the_output6;
static bridge_output6_vtbl_t vtable6;

static HRESULT o_query(IDXGIOutput *p, const GUID *iid, void **out)
{
   if (no_output6 || iid->id != 6) return -1;
   asked_output = (int)p->index;
   ++refs; *out = &the_output6;
   return 0;
}
static HRESULT o_desc(IDXGIOutput *p, DXGI_OUTPUT_DESC *desc)
{ desc->Monitor = (HMONITOR)(size_t)(p->index + 1); return 0; }
static unsigned o_release(IDXGIOutput *p) { (void)p; return --refs; }
static const struct output_vtable o_vtable = { o_query, o_desc, o_release };
static struct output outputs[4] = {
   { &o_vtable, 0 }, { &o_vtable, 1 }, { &o_vtable, 2 }, { &o_vtable, 3 } };

static HRESULT a_outputs(IDXGIAdapter1 *p, unsigned i, IDXGIOutput **out)
{
   if (i >= 2) return -1;
   ++refs; *out = &outputs[p->index * 2 + i];
   return 0;
}
static unsigned a_release(IDXGIAdapter1 *p) { (void)p; return --refs; }
static const struct adapter_vtable a_vtable = { a_outputs, a_release };
static struct adapter adapters[2] = { { &a_vtable, 0 }, { &a_vtable, 1 } };

static HRESULT f_adapters(IDXGIFactory1 *p, unsigned i, IDXGIAdapter1 **out)
{
   (void)p;
   if (i >= 2) return -1;
   ++refs; *out = &adapters[i];
   return 0;
}
static unsigned f_release(IDXGIFactory1 *p) { (void)p; return --refs; }
static const struct factory_vtable f_vtable = { f_adapters, f_release };
static struct factory the_factory = { &f_vtable };

static HRESULT WINAPI mock_create_factory(REFIID iid, void **out)
{
   assert(iid->id == 1);
   if (no_factory) return -1;
   ++factories; ++refs; *out = &the_factory;
   return 0;
}
static HMODULE LoadLibraryA(const char *name)
{ assert(!strcmp(name, "dxgi.dll")); return no_dll ? NULL : (HMODULE)&refs; }
static void *get_proc(HMODULE dll, const char *name)
{
   assert(dll && !strcmp(name, "CreateDXGIFactory1"));
   return no_entry ? NULL : (void *)&the_factory;
}
/* the entry point comes back as data here; the test hands the call on */
#define GetProcAddress(dll, name) \
   (get_proc(dll, name) ? mock_create_factory : (create_factory_t)0)
static HMONITOR MonitorFromWindow(HWND window, unsigned flags)
{ assert(window && flags == MONITOR_DEFAULTTONEAREST); return window_monitor; }

#include "window.inc"

int main(void)
{
   unsigned (**slots)(IUnknown *) = (unsigned (**)(IUnknown *))&vtable6;
   HWND window = (HWND)&refs;
   slots[2] = release6;
   vtable6.CheckHardwareCompositionSupport = check;
   the_output6.lpVtbl = &vtable6;

   /* the window's own output answers, wherever it is listed */
   supports[0] = supports[1] = supports[2] = 1;
   window_monitor = (HMONITOR)(size_t)4;       /* second adapter, second output */
   assert(!ra_dxgi_window_flips_fullscreen(window) && asked_output == 3);
   assert(refs == 0 && factories == 1);
   supports[3] = 1; supports[0] = supports[1] = supports[2] = 0;
   assert(ra_dxgi_window_flips_fullscreen(window) && asked_output == 3 && refs == 0);
   supports[3] = 2;                            /* windowed only */
   assert(!ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   supports[0] = 1;
   window_monitor = (HMONITOR)(size_t)1;       /* first of all: stops there */
   assert(ra_dxgi_window_flips_fullscreen(window) && asked_output == 0 && refs == 0);

   /* nothing to ask */
   asked_output   = -1;
   window_monitor = (HMONITOR)(size_t)9;       /* on no output listed */
   assert(!ra_dxgi_window_flips_fullscreen(window) && asked_output == -1 && refs == 0);
   window_monitor = (HMONITOR)(size_t)1;
   no_output6 = 1;
   assert(!ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   no_output6 = 0; no_factory = 1;
   assert(!ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   no_factory = 0; no_entry = 1;
   assert(!ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   no_entry = 0;
   window_monitor = NULL;
   assert(!ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   assert(!ra_dxgi_window_flips_fullscreen(NULL) && refs == 0);
   window_monitor = (HMONITOR)(size_t)1;
   bridge_dxgi_dll = NULL; no_dll = 1;
   assert(!ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   no_dll = 0;
   assert(ra_dxgi_window_flips_fullscreen(window) && refs == 0);
   puts("PASS: DXGI output of a window's monitor and its composition answer");
   return 0;
}
