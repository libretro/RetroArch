/* The bridge's question to its chain's output, and the answer it
 * gives the presenter: output.inc and ask.inc, cut by run.py. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

typedef int HRESULT;
typedef unsigned UINT;
typedef int BOOL;
typedef struct { int id; } GUID;
#define STDMETHODCALLTYPE
#define FAILED(value) ((value) < 0)
#define SUCCEEDED(value) ((value) >= 0)
#define DROP(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)

static const GUID bridge_IDXGIOutput6 = { 6 };
#include "output.inc"

typedef struct unknown IUnknown;
struct unknown_vtable { void *before[2]; unsigned (*Release)(IUnknown *); };
struct unknown { const struct unknown_vtable *lpVtbl; };

typedef struct output IDXGIOutput;
struct output_vtable {
   HRESULT (*QueryInterface)(IDXGIOutput *, const GUID *, void **);
   unsigned (*Release)(IDXGIOutput *);
};
struct output { const struct output_vtable *lpVtbl; };

typedef struct chain IDXGISwapChain3;
struct chain_vtable { HRESULT (*GetContainingOutput)(IDXGISwapChain3 *, IDXGIOutput **); };
struct chain { const struct chain_vtable *lpVtbl; };

typedef struct ra_dxgi_bridge {
   IDXGISwapChain3 *chain;
   int failed, flips;
   BOOL tearing;
   unsigned generation;
} ra_dxgi_bridge_t;

static int no_output, no_output6, check_fails, refs, asked;
static UINT support;

/* IDXGIOutput6 as the system lays it out: Release third, the method
 * asked in the slot the shipping typedef names */
static unsigned release6(IUnknown *p) { (void)p; return --refs; }
static HRESULT check(bridge_output6_t *p, UINT *flags)
{
   (void)p;
   if (check_fails) return -1;
   *flags = support;
   return 0;
}
static struct { void *lpVtbl; } the_output6;
static bridge_output6_vtbl_t vtable6;

static HRESULT query(IDXGIOutput *p, const GUID *iid, void **out)
{
   (void)p;
   if (no_output6 || iid->id != 6) return -1;
   ++refs; *out = &the_output6;
   return 0;
}
static unsigned release(IDXGIOutput *p) { (void)p; return --refs; }
static const struct output_vtable output_vtable = { query, release };
static struct output the_output = { &output_vtable };

static HRESULT containing(IDXGISwapChain3 *p, IDXGIOutput **out)
{
   (void)p;
   ++asked;
   if (no_output) return -1;
   ++refs; *out = &the_output;
   return 0;
}
static const struct chain_vtable chain_vtable = { containing };
static struct chain the_chain = { &chain_vtable };

#include "ask.inc"

int main(void)
{
   ra_dxgi_bridge_t b;
   unsigned (**slots)(IUnknown *) = (unsigned (**)(IUnknown *))&vtable6;
   assert(offsetof(bridge_output6_vtbl_t, CheckHardwareCompositionSupport)
         == 28 * sizeof(void *));
   assert(offsetof(struct unknown_vtable, Release) == 2 * sizeof(void *));
   slots[2] = release6;
   vtable6.CheckHardwareCompositionSupport = check;
   the_output6.lpVtbl = &vtable6;
   b.chain = &the_chain; b.failed = 0; b.tearing = 1; b.flips = 0;
   b.generation = 0;

   support = 1;                      /* fullscreen */
   ra_dxgi_bridge_ask_output(&b);
   assert(b.flips && ra_dxgi_bridge_immediate(&b, 0) && refs == 0);
   b.tearing = 0;
   assert(!ra_dxgi_bridge_immediate(&b, 0));
   b.tearing = 1; b.failed = 1;
   assert(!ra_dxgi_bridge_immediate(&b, 0));
   b.failed = 0;
   assert(!ra_dxgi_bridge_immediate(NULL, 0));

   support = 2;                      /* windowed only */
   ra_dxgi_bridge_ask_output(&b);
   assert(!b.flips && !ra_dxgi_bridge_immediate(&b, 0) && refs == 0);
   support = 1; check_fails = 1;
   ra_dxgi_bridge_ask_output(&b);
   assert(!b.flips && refs == 0);
   check_fails = 0; no_output6 = 1;
   b.flips = 1;
   ra_dxgi_bridge_ask_output(&b);
   assert(!b.flips && refs == 0);
   no_output6 = 0; no_output = 1;
   b.flips = 1;
   ra_dxgi_bridge_ask_output(&b);
   assert(!b.flips && refs == 0);
   no_output = 0;
   ra_dxgi_bridge_ask_output(&b);
   assert(b.flips && refs == 0);
   asked = 0;

   /* the window on another output: asked again, once */
   support = 2;
   assert(ra_dxgi_bridge_immediate(&b, 0) && asked == 0);
   assert(!ra_dxgi_bridge_immediate(&b, 1) && asked == 1 && refs == 0);
   assert(!ra_dxgi_bridge_immediate(&b, 1) && asked == 1);
   support = 1;
   assert(ra_dxgi_bridge_immediate(&b, 2) && asked == 2 && refs == 0);
   /* a chain that cannot tear has nothing to ask */
   b.tearing = 0;
   assert(!ra_dxgi_bridge_immediate(&b, 3) && asked == 2);
   puts("PASS: DXGI bridge output composition query and immediate present");
   return 0;
}
