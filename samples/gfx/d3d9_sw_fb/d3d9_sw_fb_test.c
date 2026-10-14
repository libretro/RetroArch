/* GET_CURRENT_SOFTWARE_FRAMEBUFFER on the D3D9 drivers
 * (gfx/common/d3d9_sw_fb.h): the frame texture lent locked to the core,
 * read out of without a copy, a cropped window moved to its corner,
 * the cached frame replayed out of the texture holding it, and that
 * texture kept alive past its release inside a frame until the cached
 * frame is retired. Run against a stand-in texture that checks every
 * lock is paired and never taken twice. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../gfx/common/d3d9_common.h"

#define TEX_W 512
#define TEX_H 256
#define BPP   2

static int fails;
static unsigned retires, invalidates;

void video_driver_cached_frame_retire(void)     { retires++; }
void video_driver_cached_frame_invalidate(void) { invalidates++; }

#define CHECK(cond, what) do { \
   if (cond) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } \
} while (0)

struct fake_tex
{
   IDirect3DTexture9 iface;
   uint8_t mem[TEX_W * BPP * TEX_H];
   int     locked;
   int     readonly;
   RECT    rect;        /* the last lock's, the whole texture for NULL */
   int     refs;
   int     lock_errors;
};

static IDirect3DTexture9Vtbl tex_vtbl;

static void trap(void)
{
   printf("  [FAIL] a texture method the stand-in lacks\n");
   exit(1);
}

static HRESULT STDMETHODCALLTYPE tex_lock(IDirect3DTexture9 *p, UINT level,
      D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
   struct fake_tex *t = (struct fake_tex*)p;
   (void)rect;
   if (level || t->locked)
   {
      t->lock_errors++;
      return D3DERR_INVALIDCALL;
   }
   t->locked   = 1;
   t->readonly = (flags & D3DLOCK_READONLY) ? 1 : 0;
   if (rect)
      t->rect  = *rect;
   else
   {
      t->rect.left  = 0;
      t->rect.top   = 0;
      t->rect.right = TEX_W;
      t->rect.bottom= TEX_H;
   }
   lr->pBits   = t->mem + t->rect.top * TEX_W * BPP + t->rect.left * BPP;
   lr->Pitch   = TEX_W * BPP;
   return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE tex_unlock(IDirect3DTexture9 *p, UINT level)
{
   struct fake_tex *t = (struct fake_tex*)p;
   if (level || !t->locked)
   {
      t->lock_errors++;
      return D3DERR_INVALIDCALL;
   }
   t->locked = 0;
   return D3D_OK;
}

/* No system-memory twin: a managed texture, as a device without Ex has */
static HRESULT STDMETHODCALLTYPE tex_get_private(IDirect3DTexture9 *p,
      REFGUID guid, void *data, DWORD *size)
{
   (void)p; (void)guid; (void)data; (void)size;
   return D3DERR_NOTFOUND;
}

static ULONG STDMETHODCALLTYPE tex_release(IDirect3DTexture9 *p)
{
   return (ULONG)--((struct fake_tex*)p)->refs;
}

static void tex_init(struct fake_tex *t, uint8_t fill)
{
   memset(t, 0, sizeof(*t));
   memset(t->mem, fill, sizeof(t->mem));
   t->iface.lpVtbl = &tex_vtbl;
   t->refs         = 1;
}

static LPDIRECT3DTEXTURE9 tx(struct fake_tex *t)
{
   return (LPDIRECT3DTEXTURE9)&t->iface;
}

/* The last lock covered @w x @h from the corner */
static int locked_as(struct fake_tex *t, unsigned w, unsigned h)
{
   return !t->rect.left && !t->rect.top
      && t->rect.right == (LONG)w && t->rect.bottom == (LONG)h;
}

/* The pattern a core writes: pixel (x, y) of frame @n */
static uint16_t px(unsigned n, unsigned x, unsigned y)
{
   return (uint16_t)(n * 977 + x * 31 + y * 7 + 1);
}

static void write_frame(uint8_t *base, size_t pitch, unsigned n,
      unsigned w, unsigned h)
{
   unsigned x, y;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         ((uint16_t*)(base + y * pitch))[x] = px(n, x, y);
}

/* The texture holds frame @n's @w x @h window from (@ox, @oy) at its
 * corner, and zeros past it */
static int holds(struct fake_tex *t, unsigned n, unsigned ox, unsigned oy,
      unsigned w, unsigned h)
{
   unsigned x, y;
   for (y = 0; y < TEX_H; y++)
      for (x = 0; x < TEX_W; x++)
      {
         uint16_t v = ((uint16_t*)(t->mem + y * TEX_W * BPP))[x];
         uint16_t e = (x < w && y < h) ? px(n, ox + x, oy + y) : 0;
         if (v != e)
            return 0;
      }
   return 1;
}

int main(void)
{
   static struct fake_tex a, b, c;
   struct retro_framebuffer fb;
   d3d9_sw_fb_t sw;
   unsigned dims = VIDEO_SCALE_PACK(TEX_W, TEX_H);
   unsigned pitch;
   const uint8_t *src;
   size_t i;

   {
      void **slot = (void**)&tex_vtbl;
      for (i = 0; i < sizeof(tex_vtbl) / sizeof(void*); i++)
         slot[i] = (void*)trap;
      tex_vtbl.LockRect   = tex_lock;
      tex_vtbl.UnlockRect = tex_unlock;
      tex_vtbl.Release    = tex_release;
      tex_vtbl.GetPrivateData = tex_get_private;
   }
   memset(&sw, 0, sizeof(sw));
   tex_init(&a, 0xAB);
   tex_init(&b, 0xAB);
   tex_init(&c, 0xAB);

   printf("lending:\n");
   memset(&fb, 0, sizeof(fb));
   fb.width  = TEX_W + 1;
   fb.height = 240;
   CHECK(!d3d9_sw_fb_lend(&sw, tx(&a), dims, BPP, &fb) && !a.locked,
         "a framebuffer wider than the texture is not lent");
   fb.width  = 0;
   CHECK(!d3d9_sw_fb_lend(&sw, tx(&a), dims, BPP, &fb),
         "nor one of no size");
   fb.width  = 320;
   CHECK(d3d9_sw_fb_lend(&sw, tx(&a), dims, BPP, &fb)
         && fb.data == a.mem && fb.pitch == TEX_W * BPP && a.locked
         && fb.format == RETRO_PIXEL_FORMAT_RGB565
         && (fb.memory_flags & RETRO_MEMORY_TYPE_CACHED),
         "the texture is lent locked, its rows at its own pitch");
   CHECK(locked_as(&a, 320, 240),
         "only the size asked for is locked, so only it goes up");
   CHECK(d3d9_sw_fb_lend(&sw, tx(&a), dims, BPP, &fb) && fb.data == a.mem
         && !a.lock_errors,
         "asked again before a frame, the same loan");

   printf("a frame written into the loan:\n");
   write_frame(a.mem, TEX_W * BPP, 1, 320, 240);
   d3d9_sw_fb_settle(&sw, a.mem, 320, 240, TEX_W * BPP, BPP, 0);
   CHECK(!a.locked && sw.frame_tex == tx(&a) && sw.frame == a.mem,
         "is left where it is, the texture unlocked");
   CHECK(holds(&a, 1, 0, 0, 320, 240),
         "and past it is cleared, the size being new");
   pitch = 0;
   CHECK(!d3d9_sw_fb_src_begin(&sw, a.mem, tx(&a), &pitch),
         "the upload into that texture copies nothing");

   printf("a frame of the same size again:\n");
   tex_init(&b, 0xAB);
   CHECK(d3d9_sw_fb_lend(&sw, tx(&b), dims, BPP, &fb), "b is lent");
   write_frame(b.mem, TEX_W * BPP, 2, 320, 240);
   d3d9_sw_fb_settle(&sw, b.mem, 320, 240, TEX_W * BPP, BPP,
         VIDEO_SCALE_PACK(320, 240));
   CHECK(((uint16_t*)b.mem)[320] == 0xABAB && locked_as(&b, 320, 240),
         "nothing past it is touched, nor locked, when its size is the "
         "texture's last");

   printf("a cropped window inside the loan:\n");
   tex_init(&c, 0xAB);
   fb.width = 320; fb.height = 240;
   CHECK(d3d9_sw_fb_lend(&sw, tx(&c), dims, BPP, &fb), "c is lent");
   write_frame(c.mem, TEX_W * BPP, 3, 320, 240);
   d3d9_sw_fb_settle(&sw, c.mem + 4 * TEX_W * BPP + 8 * BPP, 304, 232,
         TEX_W * BPP, BPP, VIDEO_SCALE_PACK(320, 240));
   CHECK(holds(&c, 3, 8, 4, 304, 232) && !c.locked,
         "is moved to the corner, what is past it cleared");
   CHECK(locked_as(&c, TEX_W, TEX_H),
         "under a lock of the whole texture");

   printf("a window at a narrower pitch inside the loan:\n");
   tex_init(&c, 0xAB);
   CHECK(d3d9_sw_fb_lend(&sw, tx(&c), dims, BPP, &fb), "c is lent");
   write_frame(c.mem + 16, 320 * BPP, 4, 300, 200);
   d3d9_sw_fb_settle(&sw, c.mem + 16, 300, 200, 320 * BPP, BPP, 0);
   CHECK(holds(&c, 4, 0, 0, 300, 200) && !c.locked,
         "is moved row by row without overrunning itself");

   printf("a frame from elsewhere:\n");
   {
      static uint8_t own[320 * BPP * 240];
      tex_init(&b, 0xAB);
      CHECK(d3d9_sw_fb_lend(&sw, tx(&b), dims, BPP, &fb), "b is lent");
      d3d9_sw_fb_settle(&sw, own, 320, 240, 320 * BPP, BPP, 0);
      CHECK(!b.locked && !sw.tex && sw.frame_tex == tx(&c)
            && b.mem[0] == 0xAB,
            "ends the loan, the texture as it was");
      CHECK(d3d9_sw_fb_src_begin(&sw, own, tx(&b), &pitch) == own,
            "and the upload copies it");
   }

   printf("the cached frame replayed:\n");
   pitch = 0;
   src   = d3d9_sw_fb_src_begin(&sw, c.mem + 16, tx(&b), &pitch);
   CHECK(src == c.mem && pitch == TEX_W * BPP && c.locked && c.readonly,
         "into another texture, read out of the one holding it");
   d3d9_sw_fb_src_end(&sw, c.mem + 16, tx(&b));
   CHECK(!c.locked && sw.frame_tex == tx(&b),
         "which is unlocked, and the copy replayed from next");

   printf("the texture holding it released inside a frame:\n");
   c.refs = 1;
   b.refs = 1;
   retires = invalidates = 0;
   d3d9_sw_fb_release(&sw, tx(&c));
   d3d9_sw_fb_release(&sw, tx(&b));
   CHECK(c.refs == 0, "another texture goes");
   CHECK(b.refs == 1 && sw.parked[0] == tx(&b) && !retires,
         "the one holding the cached frame is kept, nothing waited for");
   CHECK(d3d9_sw_fb_src_begin(&sw, c.mem + 16, tx(&a), &pitch) == b.mem,
         "and a replay still reads it");
   d3d9_sw_fb_src_end(&sw, c.mem + 16, tx(&a));
   d3d9_sw_fb_release(&sw, tx(&a));
   d3d9_sw_fb_collect(&sw);
   CHECK(retires == 1 && b.refs == 0 && !sw.parked[0],
         "outside a frame the cached frame is retired and it goes");

   printf("the upload of a frame from elsewhere:\n");
   {
      static uint8_t own[320 * BPP * 240];
      write_frame(own, 320 * BPP, 5, 320, 240);
      tex_init(&a, 0xAB);
      d3d9_frame_upload(tx(&a), dims, 0, own, 320 * BPP, 320, 240, BPP);
      CHECK(holds(&a, 5, 0, 0, 320, 240) && locked_as(&a, TEX_W, TEX_H)
            && !a.locked,
            "at a new size, the whole texture: the frame, past it cleared");
      write_frame(own, 320 * BPP, 6, 320, 240);
      d3d9_frame_upload(tx(&a), dims, VIDEO_SCALE_PACK(320, 240), own,
            320 * BPP, 320, 240, BPP);
      CHECK(holds(&a, 6, 0, 0, 320, 240) && locked_as(&a, 320, 240),
            "at the same size, only the frame's rectangle is locked");
   }

   printf("lending the texture the cached frame is in:\n");
   tex_init(&a, 0xAB);
   sw.frame     = a.mem;
   sw.frame_tex = tx(&a);
   CHECK(d3d9_sw_fb_lend(&sw, tx(&a), dims, BPP, &fb)
         && !sw.frame && !sw.frame_tex,
         "forgets it, the core writing over it");
   d3d9_sw_fb_drop(&sw);
   CHECK(!a.locked && !sw.tex, "a loan dropped is unlocked");

   CHECK(!a.lock_errors && !b.lock_errors && !c.lock_errors,
         "every lock paired, none taken twice");
   CHECK(!invalidates, "the cached frame never dropped without a wait");
   return fails ? 1 : 0;
}
