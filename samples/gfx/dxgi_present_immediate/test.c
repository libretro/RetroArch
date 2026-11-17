/* A swap chain, its output and IDXGIOutput6 as the shipping functions
 * use them, and those functions on top (shipping.inc, cut by run.py). */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

typedef long HRESULT;
typedef unsigned UINT;
typedef struct { int id; } GUID;
#define SUCCEEDED(hr) ((hr) >= 0)
#define FAILED(hr)    ((hr) < 0)
#define DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN 1
#define DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED   2

static const GUID libretro_IID_IDXGIOutput6 = { 6 };

/* what the output answers, and what was asked of it */
static bool     out_has_output6 = true;
static bool     out_check_fails;
static bool     chain_no_output;
static UINT     out_support;
static unsigned out_asked;
static int      out_refs;

struct output6;
struct output6_vtbl
{
   HRESULT (*CheckHardwareCompositionSupport)(struct output6*, UINT*);
   unsigned (*Release)(struct output6*);
};
struct output6 { const struct output6_vtbl *lpVtbl; };
typedef struct output6 *DXGIOutput6;

struct output;
struct output_vtbl
{
   HRESULT (*QueryInterface)(struct output*, const GUID*, void**);
   unsigned (*Release)(struct output*);
};
struct output { const struct output_vtbl *lpVtbl; };
typedef struct output *DXGIOutput;

struct chain { int unused; };
typedef struct chain *DXGISwapChain;

static HRESULT o6_check(struct output6 *o, UINT *flags)
{
   (void)o;
   if (out_check_fails)
      return -1;
   *flags = out_support;
   return 0;
}
static unsigned o6_release(struct output6 *o) { (void)o; return --out_refs; }
static const struct output6_vtbl o6_vtbl = { o6_check, o6_release };
static struct output6 the_output6        = { &o6_vtbl };

static HRESULT o_query(struct output *o, const GUID *iid, void **out)
{
   (void)o;
   if (!out_has_output6 || iid->id != 6)
      return -1;
   out_refs++;
   *out = &the_output6;
   return 0;
}
static unsigned o_release(struct output *o) { (void)o; return --out_refs; }
static const struct output_vtbl o_vtbl = { o_query, o_release };
static struct output the_output        = { &o_vtbl };

static HRESULT DXGIGetContainingOutput(DXGISwapChain chain, DXGIOutput *out)
{
   (void)chain;
   out_asked++;
   if (chain_no_output)
      return -1;
   out_refs++;
   *out = &the_output;
   return 0;
}
#define Release(x) ((x)->lpVtbl->Release(x))

typedef struct
{
   uint32_t flags;
   DXGISwapChain swapChain;
   unsigned flips_generation;
} d3d11_video_t;

typedef struct
{
   uint32_t flags;
   struct { DXGISwapChain handle; } chain;
   unsigned flips_generation;
} d3d12_video_t;

/* the outputs the window has been on */
#define HAVE_D3DKMT 1
static unsigned generation;
static unsigned win32_d3dkmt_generation(void) { return generation; }

#include "shipping.inc"

static unsigned failures;
#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         failures++; \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
      } \
   } while (0)

int main(void)
{
   unsigned i;
   struct chain c;
   d3d11_video_t d3d11;
   d3d12_video_t d3d12;
   c.unused = 0;

   /* ---- the output's answer ---- */
   out_support = DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN;
   CHECK(dxgi_chain_flips_fullscreen(&c), "an output that flips fullscreen");
   out_support = DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED;
   CHECK(!dxgi_chain_flips_fullscreen(&c), "windowed support is not fullscreen's");
   out_support = 0;
   CHECK(!dxgi_chain_flips_fullscreen(&c), "an output DWM composes");
   out_support = DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN;
   out_has_output6 = false;
   CHECK(!dxgi_chain_flips_fullscreen(&c), "no IDXGIOutput6: unknown is no");
   out_has_output6 = true;
   out_check_fails = true;
   CHECK(!dxgi_chain_flips_fullscreen(&c), "a failed query is no");
   out_check_fails = false;
   chain_no_output = true;
   CHECK(!dxgi_chain_flips_fullscreen(&c), "no containing output is no");
   chain_no_output = false;
   CHECK(!dxgi_chain_flips_fullscreen(NULL), "no chain is no");
   CHECK(out_refs == 0, "an output or its IDXGIOutput6 was left referenced");

   /* ---- D3D11: tearing allowed, vsync off, and past the compositor ---- */
   d3d11.swapChain = &c;
   d3d11.flags     = D3D11_ST_FLAG_HAS_ALLOW_TEARING | D3D11_ST_FLAG_VSYNC;
   d3d11.flips_generation = 0;
   out_asked       = 0;
   CHECK(!d3d11_present_immediate(&d3d11) && !out_asked,
         "d3d11: vsync on is not immediate, and asks nothing");
   d3d11.flags     = 0;
   CHECK(!d3d11_present_immediate(&d3d11) && !out_asked,
         "d3d11: without tearing it is not immediate, and asks nothing");
   d3d11.flags     = D3D11_ST_FLAG_HAS_ALLOW_TEARING;
   for (i = 0; i < 100; i++)
      CHECK(d3d11_present_immediate(&d3d11), "d3d11: tearing past the compositor");
   CHECK(out_asked == 1, "d3d11: the output is asked once, not every frame");
   /* the chain resized onto an output DWM composes */
   out_support  = 0;
   d3d11.flags &= ~D3D11_ST_FLAG_FLIPS_ASKED;
   CHECK(!d3d11_present_immediate(&d3d11) && out_asked == 2,
         "d3d11: composed after a resize is not immediate");
   /* the window moved, its chain as it was, to an output that flips */
   out_support  = DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN;
   CHECK(!d3d11_present_immediate(&d3d11) && out_asked == 2,
         "d3d11: asked again with nothing changed");
   generation++;
   for (i = 0; i < 100; i++)
      CHECK(d3d11_present_immediate(&d3d11),
            "d3d11: on another output its answer is not asked for");
   CHECK(out_asked == 3, "d3d11: another output is asked once");
   CHECK(!d3d11_present_immediate(NULL), "d3d11: no driver");

   /* ---- D3D12 ---- */
   out_support       = DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN;
   d3d12.chain.handle = &c;
   d3d12.flags       = D3D12_ST_FLAG_VSYNC;
   d3d12.flips_generation = generation;
   out_asked         = 0;
   CHECK(!d3d12_present_immediate(&d3d12) && !out_asked,
         "d3d12: vsync on is not immediate, and asks nothing");
   d3d12.flags       = 0;
   for (i = 0; i < 100; i++)
      CHECK(d3d12_present_immediate(&d3d12), "d3d12: tearing past the compositor");
   CHECK(out_asked == 1, "d3d12: the output is asked once, not every frame");
   out_support  = 0;
   d3d12.flags &= ~D3D12_ST_FLAG_FLIPS_ASKED;
   CHECK(!d3d12_present_immediate(&d3d12) && out_asked == 2,
         "d3d12: composed after a resize is not immediate");
   out_support  = DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN;
   CHECK(!d3d12_present_immediate(&d3d12) && out_asked == 2,
         "d3d12: asked again with nothing changed");
   generation++;
   for (i = 0; i < 100; i++)
      CHECK(d3d12_present_immediate(&d3d12),
            "d3d12: on another output its answer is not asked for");
   CHECK(out_asked == 3, "d3d12: another output is asked once");
   CHECK(!d3d12_present_immediate(NULL), "d3d12: no driver");
   CHECK(out_refs == 0, "an output was left referenced by a driver's ask");

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("PASS D3D11/D3D12 immediate present: tearing, vsync and the output's composition\n");
   return 0;
}
