/* The threaded wrapper's D3D11 hardware ring: what the video thread
 * asks of the driver before it draws a hardware frame.
 *
 * Version 1 cores record into a deferred context; the driver must
 * replay the slot's command list and take its texture
 * (hw_ring_present_slot). Version 2 cores hand their texture over
 * directly (hw_ring_install). A version 1 core given install instead
 * reaches the driver with no texture, and the driver reads
 * RETRO_HW_FRAME_BUFFER_VALID as pixels.
 *
 * gfx/video_thread_hw.c is included whole and driven through its own
 * entry points against a recording poke; no device is created.
 *
 *   d3d11_ring_dispatch_test     both versions, must pass
 */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#include <stdio.h>
#include <string.h>

#include "../../../gfx/video_thread_hw.c"

#define FRAMES 7

static video_driver_state_t   fake_video_st;
static settings_t             fake_settings;
static video_poke_interface_t fake_poke;
static thread_video_t         fake_thr;
static struct retro_hw_render_interface_d3d11 fake_real;
static struct retro_hw_render_context_negotiation_interface_d3d11 fake_neg;

static unsigned n_present[VIDEO_THREAD_HW_RING];
static unsigned n_install;
static unsigned n_capture;
static const void *installed;

video_driver_state_t *video_state_get_ptr(void) { return &fake_video_st; }
settings_t *config_get_ptr(void) { return &fake_settings; }
void video_thread_call_on_waiter(void (*fn)(void*), void *data) { fn(data); }
void video_thread_main_pump(void) { }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

static bool r_lock(void *h) { (void)h; return true; }
static void r_unlock(void *h) { (void)h; }

static bool p_get_iface(void *d, const struct retro_hw_render_interface **i)
{
   (void)d;
   *i = (const struct retro_hw_render_interface*)&fake_real;
   return true;
}
static bool p_fence_new(void *d, void **f) { (void)d; *f = (void*)1; return true; }
static void p_fence_free(void *d, void *f) { (void)d; (void)f; }
static void p_fence_signal(void *d, void *f) { (void)d; (void)f; }
static bool p_fence_wait(void *d, void *f, unsigned t) { (void)d; (void)f; (void)t; return true; }
static bool p_context_new(void *d, void **c) { (void)d; *c = (void*)&fake_real; return true; }
static void p_context_free(void *d, void *c) { (void)d; (void)c; }
static bool p_capture(void *d, unsigned slot, const void *src, unsigned fmt)
{
   (void)d; (void)slot; (void)src; (void)fmt;
   n_capture++;
   return true;
}
static bool p_present_slot(void *d, unsigned slot)
{
   (void)d;
   if (slot < VIDEO_THREAD_HW_RING)
      n_present[slot]++;
   return true;
}
static bool p_install(void *d, const void *image, const void *sem,
      unsigned n_sem, unsigned qf, const void *cmd, unsigned n_cmd)
{
   (void)d; (void)sem; (void)n_sem; (void)qf; (void)cmd; (void)n_cmd;
   n_install++;
   installed = image;
   return image != NULL;
}

/* A texture is only ever AddRef'd and Released by the ring. */
static LONG tex_refs;
static ULONG STDMETHODCALLTYPE t_addref(ID3D11Texture2D *t)
{ (void)t; return (ULONG)++tex_refs; }
static ULONG STDMETHODCALLTYPE t_release(ID3D11Texture2D *t)
{ (void)t; return (ULONG)--tex_refs; }
static ID3D11Texture2DVtbl tex_vtbl;
static ID3D11Texture2D     tex;

static void setup(unsigned version)
{
   memset(&fake_thr, 0, sizeof(fake_thr));
   memset(&fake_real, 0, sizeof(fake_real));
   memset(&fake_neg, 0, sizeof(fake_neg));
   memset(n_present, 0, sizeof(n_present));
   n_install = n_capture = 0;
   installed = NULL;

   fake_poke.get_hw_render_interface = p_get_iface;
   fake_poke.hw_ring_fence_new       = p_fence_new;
   fake_poke.hw_ring_fence_free      = p_fence_free;
   fake_poke.hw_ring_fence_signal    = p_fence_signal;
   fake_poke.hw_ring_fence_wait      = p_fence_wait;
   fake_poke.hw_ring_context_new     = p_context_new;
   fake_poke.hw_ring_context_free    = p_context_free;
   fake_poke.hw_ring_capture         = p_capture;
   fake_poke.hw_ring_present_slot    = p_present_slot;
   fake_poke.hw_ring_install         = p_install;

   fake_thr.poke        = &fake_poke;
   fake_thr.driver_data = &fake_thr;
   retro_atomic_int_init(&fake_thr.frame.state, 0);

   fake_real.interface_type    = RETRO_HW_RENDER_INTERFACE_D3D11;
   fake_real.interface_version = version;
   fake_real.lock_context      = r_lock;
   fake_real.unlock_context    = r_unlock;

   fake_neg.interface_type    = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11;
   fake_neg.interface_version = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_D3D11_VERSION;
   fake_neg.max_render_interface_version = version;
   fake_video_st.hw_render_context_negotiation =
      (const struct retro_hw_render_context_negotiation_interface*)&fake_neg;
}

static int run(unsigned version)
{
   const struct retro_hw_render_interface *gi = NULL;
   const struct retro_hw_render_interface_d3d11 *iface;
   unsigned want[VIDEO_THREAD_HW_RING];
   unsigned f, i, taken_back;
   int bad = 0;

   setup(version);
   memset(want, 0, sizeof(want));
   if (!video_thread_get_hw_render_interface(&fake_thr, &gi) || !gi)
   {
      printf("v%u: no interface\n", version);
      return 1;
   }
   iface = (const struct retro_hw_render_interface_d3d11*)gi;
   if (iface->interface_version != version)
   {
      printf("v%u: core got version %u\n", version, iface->interface_version);
      return 1;
   }

   for (f = 0; f < FRAMES; f++)
   {
      int slot;
      unsigned inst0 = n_install;
      if (version >= RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2)
      {
         iface->lock_context(iface->handle);
         iface->set_texture(iface->handle, &tex);
         iface->unlock_context(iface->handle);
      }
      slot = video_thread_hw_publish(&fake_thr, &taken_back);
      if (slot < 0 || slot >= VIDEO_THREAD_HW_RING)
      {
         printf("v%u frame %u: publish gave slot %d\n", version, f, slot);
         return 1;
      }
      video_thread_hw_before_frame(&fake_thr, slot, false);
      video_thread_hw_after_frame(&fake_thr, slot);

      if (version >= RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2)
      {
         if (n_install != inst0 + 1 || installed != &tex)
         {
            printf("v%u frame %u: texture not installed\n", version, f);
            bad = 1;
         }
      }
      else
      {
         want[slot]++;
         if (n_install != inst0)
         {
            printf("v%u frame %u: install called for a deferred-context core\n",
                  version, f);
            bad = 1;
         }
      }
   }

   for (i = 0; i < VIDEO_THREAD_HW_RING; i++)
      if (n_present[i] != want[i])
      {
         printf("v%u: slot %u replayed %u times, want %u\n",
               version, i, n_present[i], want[i]);
         bad = 1;
      }
   if (version < RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2
         && n_capture != FRAMES)
   {
      printf("v1: %u captures for %u frames\n", n_capture, FRAMES);
      bad = 1;
   }

   video_thread_hw_free(&fake_thr);
   return bad;
}

int main(void)
{
   int bad = 0;
   tex_vtbl.AddRef  = t_addref;
   tex_vtbl.Release = t_release;
   tex.lpVtbl       = &tex_vtbl;
   strlcpy(fake_settings.arrays.video_driver, "d3d11",
         sizeof(fake_settings.arrays.video_driver));
   fake_video_st.hw_render.context_type = RETRO_HW_CONTEXT_D3D11;

   bad |= run(RETRO_HW_RENDER_INTERFACE_D3D11_VERSION);
   bad |= run(RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2);
   if (tex_refs != 0)
   {
      printf("texture left with %ld references\n", (long)tex_refs);
      bad = 1;
   }
   printf("d3d11_ring_dispatch: %s\n", bad ? "FAIL" : "ok");
   return bad;
}
