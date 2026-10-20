/* Production frontend/D3D/Vulkan handoff without a window or graphics SDK. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <formats/image.h>
#include <formats/video_hw_frame.h>
#include "gfx/video_defines.h"
#include "frame.inc"
#ifndef HAVE_THREADS
#define HAVE_THREADS
#endif
#include <rthreads/rthreads.h>
#define HAVE_VIDEO_FILTER
#define RETRO_HW_FRAME_BUFFER_VALID ((void*)-1)
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED, VIDEO_TEXTURE_UPDATE_DONE, VIDEO_TEXTURE_UPDATE_DROPPED };
typedef struct { uint32_t import_formats; } video_device_info_t;
typedef struct { enum video_texture_update (*import_movie_frame)(void*, const video_frame_import_t*); } video_poke_interface_t;
typedef struct
{
   const video_poke_interface_t *poke;
   void *data, *state_filter, *record_gpu_buffer;
   bool movie_frame, movie_refused;
} video_driver_state_t;
#include "gfx/video_thread_movie.h"
typedef struct
{
   const video_poke_interface_t *poke;
   void *driver_data;
   retro_atomic_int_t movie_capable;
   video_thread_movie_state_t movie_state;
   video_thread_movie_t movie_next;
   bool self;
} thread_video_t;
static thread_video_t *video_thread_thr_capture;
static bool video_thread_is_self(thread_video_t *thr) { return thr->self; }
#include "wrapper.inc"
static video_driver_state_t video_driver_st;
static bool threaded, device_ok = true;
static uint32_t formats = IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010;
static struct { bool enable, streaming_enable; } recorder;
static unsigned failures, abandoned, imports, invalidated;
static enum video_texture_update outcome;
static video_frame_import_t captured;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)
static bool video_driver_thread_wrapper_active(void) { return threaded; }

static bool video_driver_get_device(video_device_info_t *info) { info->import_formats = formats; return device_ok; }
#define recording_state_get_ptr() (&recorder)
static void video_driver_cached_frame_invalidate(void) { invalidated++; }
static enum video_texture_update import(void *data, const video_frame_import_t *f)
{
   CHECK(data == &imports);
   imports++;
   if (f) captured = *f;
   return outcome;
}
#include "front.inc"
static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &abandoned && done == 88 && value == 7);
   abandoned++;
}
static void frontend(void)
{
   video_hw_frame_t hw;
   video_poke_interface_t poke;
   thread_video_t thr;
   unsigned before;
   memset(&thr, 0, sizeof(thr));
   retro_atomic_int_init(&thr.movie_capable, 1);
   video_thread_movie_state_init(&thr.movie_state);
   video_thread_thr_capture = &thr;
   memset(&hw, 0, sizeof(hw)); memset(&poke, 0, sizeof(poke));
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   poke.import_movie_frame = import; video_driver_st.poke = &poke;
   video_driver_st.data = &imports;
   CHECK(video_driver_movie_refresh(NULL, NULL));
   threaded = true; CHECK(video_driver_movie_refresh(NULL, NULL));
   retro_atomic_store_release_int(&thr.movie_capable, 0); CHECK(!video_driver_movie_refresh(NULL, NULL));
   retro_atomic_store_release_int(&thr.movie_capable, 1); threaded = false;
   video_driver_st.state_filter = &imports;
   CHECK(!video_driver_movie_refresh(NULL, NULL)); video_driver_st.state_filter = NULL;
   recorder.enable = true; CHECK(!video_driver_movie_refresh(NULL, NULL));
   video_driver_st.record_gpu_buffer = &imports;
   CHECK(video_driver_movie_refresh(NULL, NULL)); recorder.enable = false;
   video_driver_st.record_gpu_buffer = NULL; recorder.streaming_enable = true;
   CHECK(!video_driver_movie_refresh(NULL, NULL)); recorder.streaming_enable = false;
   device_ok = false; CHECK(!video_driver_movie_refresh(NULL, NULL)); device_ok = true;
   formats = 0; CHECK(!video_driver_movie_refresh(NULL, NULL)); formats = IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010;
   hw.image = 12; hw.wait = 34; hw.wait_value = 56; hw.done = 88; hw.done_value = 7;
   hw.width = 640; hw.height = 480; hw.layout = 3; hw.yuv = 1;
   hw.plane_views[0] = 11; hw.plane_views[1] = 12;
   hw.array_slice = 7; hw.crop_x = 2; hw.crop_y = 4;
   hw.owner = &abandoned; hw.abandon = abandon;
   outcome = VIDEO_TEXTURE_UPDATE_DROPPED;
   CHECK(!video_driver_movie_refresh(NULL, &hw) && !abandoned && !video_driver_st.movie_frame);
   CHECK(captured.pixfmt == IMAGE_PIXFMT_NV12 && captured.dims == VIDEO_SCALE_PACK(640,480)
         && captured.image == hw.image && captured.wait == hw.wait && captured.wait_value == hw.wait_value
         && captured.done == hw.done && captured.done_value == hw.done_value
         && captured.owner == hw.owner && captured.abandon == hw.abandon && captured.layout == hw.layout
         && captured.plane_views[0] == hw.plane_views[0] && captured.plane_views[1] == hw.plane_views[1]
         && captured.array_slice == hw.array_slice && captured.crop_x == hw.crop_x
         && captured.crop_y == hw.crop_y);
   hw.pixfmt = IMAGE_PIXFMT_P010; outcome = VIDEO_TEXTURE_UPDATE_DONE;
   CHECK(video_driver_movie_refresh(NULL, &hw) && !abandoned && video_driver_st.movie_frame);
   CHECK(captured.pixfmt == IMAGE_PIXFMT_P010);
   outcome = VIDEO_TEXTURE_UPDATE_REFUSED;
   CHECK(video_driver_movie_refresh(NULL, &hw) && abandoned == 1);
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   outcome = VIDEO_TEXTURE_UPDATE_DONE;
   CHECK(video_driver_movie_refresh(NULL, &hw) && abandoned == 2);
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   threaded = true;
   before = imports;
   video_driver_movie_clear();
   CHECK(imports == before + 1);
   CHECK(video_driver_movie_refresh(NULL, NULL));
   threaded = false;
   video_driver_movie_clear(); CHECK(!video_driver_st.movie_frame && invalidated == 1);
}

typedef struct resource resource;
typedef struct { void (*AddRef)(resource*); void (*Release)(resource*); } resource_vtable;
struct resource { resource_vtable *lpVtbl; unsigned refs; };
static void add_ref(resource *r) { r->refs++; }
static void release(resource *r) { CHECK(r->refs); r->refs--; }
static resource_vtable methods = { add_ref, release };
static resource image = { &methods, 1 };
#define Release(p) do { if (p) (p)->lpVtbl->Release(p); } while (0)
typedef struct { unsigned Format, Width; } desc_t;
typedef struct { resource *handle; desc_t desc; } d3d11_texture_t;
typedef d3d11_texture_t d3d12_texture_t;
static d3d11_texture_t converted = { &image, { 2, 640 } };
#define D3D11_ST_FLAG_INIT_HISTORY 1
#define D3D11_ST_FLAG_RESIZE_RTS 2
#define D3D12_ST_FLAG_INIT_HISTORY 1
#define D3D12_ST_FLAG_RESIZE_RTS 2
typedef struct
{
   uintptr_t movie_texture; bool movie_pending;
   struct { resource *texture; } hw_v2;
   struct { void *view; bool eligible; } hw_direct;
   struct { struct { desc_t desc; } history[1]; } chain;
   unsigned format, flags;
} d3d11_video_t;
typedef struct
{
   uintptr_t movie_texture; bool movie_pending, movie_importing;
   resource *hw_render_texture; unsigned hw_render_texture_format;
   struct { resource *current; struct { uint64_t ptr; } current_gpu; bool eligible; } hw_direct;
   struct { struct { desc_t desc; } history[1]; } filter;
   unsigned format, flags;
} d3d12_video_t;
static enum video_texture_update d3d11_import_texture(void *v, uintptr_t *id,
      const video_frame_import_t *f, enum texture_filter_type filter)
{ (void)v; (void)f; CHECK(filter == TEXTURE_FILTER_LINEAR); if (outcome == VIDEO_TEXTURE_UPDATE_DONE) *id = (uintptr_t)&converted; return outcome; }
static enum video_texture_update d3d12_import_texture(void *v, uintptr_t *id,
      const video_frame_import_t *f, enum texture_filter_type filter)
{ CHECK(((d3d12_video_t*)v)->movie_importing); return d3d11_import_texture(v, id, f, filter); }
static void d3d11_gfx_unload_texture_internal(d3d11_video_t *v, uintptr_t id)
{ (void)v; if (id) { CHECK(id == (uintptr_t)&converted); release(&image); } }
static void d3d12_gfx_unload_texture_internal(d3d12_video_t *v, uintptr_t id)
{ (void)v; d3d11_gfx_unload_texture_internal(NULL, id); }
#include "d3d11.inc"
#include "d3d12.inc"
#undef Release
static void drivers(void)
{
   d3d11_video_t v11;
   d3d12_video_t v12;
   video_frame_import_t f;
   memset(&v11, 0, sizeof(v11)); memset(&v12, 0, sizeof(v12)); memset(&f, 0, sizeof(f));
   v11.format = v12.format = 1;
   CHECK(d3d11_movie_prepare(&v11, NULL) == NULL);
   outcome = VIDEO_TEXTURE_UPDATE_DROPPED;
   CHECK(d3d11_import_movie_frame(&v11, &f) == outcome && !v11.movie_pending);
   outcome = VIDEO_TEXTURE_UPDATE_DONE;
   CHECK(d3d11_import_movie_frame(&v11, &f) == outcome && v11.movie_pending);
   CHECK(d3d11_movie_prepare(&v11, NULL) == RETRO_HW_FRAME_BUFFER_VALID
         && v11.hw_v2.texture == &image && image.refs == 2 && !v11.movie_pending && v11.hw_direct.eligible);
   /* Replacing an unconsumed present reference (e.g. minimized window). */
   CHECK(d3d11_movie_prepare(&v11, RETRO_HW_FRAME_BUFFER_VALID) == RETRO_HW_FRAME_BUFFER_VALID && image.refs == 2);
   release(v11.hw_v2.texture); v11.hw_v2.texture = NULL;
   CHECK(d3d11_movie_prepare(&v11, NULL) == NULL);
   CHECK(d3d11_movie_prepare(&v11, &f) == &f && !v11.movie_texture && image.refs == 0
         && v11.chain.history[0].desc.Format == 1 && v11.chain.history[0].desc.Width == 0 && v11.flags == 3);
   image.refs = 1;
   CHECK(d3d12_import_movie_frame(&v12, &f) == VIDEO_TEXTURE_UPDATE_DONE && !v12.movie_importing);
   CHECK(d3d12_movie_prepare(&v12, NULL) == RETRO_HW_FRAME_BUFFER_VALID
         && v12.hw_render_texture == &image && v12.hw_render_texture_format == 2 && v12.hw_direct.eligible);
   CHECK(d3d12_movie_prepare(&v12, NULL) == NULL);
   CHECK(d3d12_movie_prepare(&v12, RETRO_HW_FRAME_BUFFER_VALID) == RETRO_HW_FRAME_BUFFER_VALID);
   CHECK(d3d12_movie_prepare(&v12, &f) == &f && !v12.movie_texture && image.refs == 0
         && v12.filter.history[0].desc.Format == 1 && v12.filter.history[0].desc.Width == 0 && v12.flags == 3);
}
#define VK_FLAG_HW_ENABLE 1
struct vk_texture { unsigned image, view, layout, dims, format; };
struct vulkan_filter_chain_texture
{
   unsigned image, view, layout, dims, format, padded_dims, origin;
};
typedef struct { unsigned flags; uintptr_t movie_texture; bool movie_pending; } vk_t;
static struct vk_texture movie = { 1, 2, 3, 4, 5 };
static unsigned retired;
static void vulkan_unload_texture_internal(vk_t *vk, uintptr_t id)
{ (void)vk; if (id) { CHECK(id == (uintptr_t)&movie); retired++; } }
static enum video_texture_update vulkan_import_texture(void *vk, uintptr_t *id,
      const video_frame_import_t *f, enum texture_filter_type filter)
{
   (void)vk; CHECK(filter == TEXTURE_FILTER_LINEAR);
   if (outcome == VIDEO_TEXTURE_UPDATE_DONE)
   { movie.dims = f->dims; *id = (uintptr_t)&movie; }
   return outcome;
}
#include "vulkan.inc"
static void vulkan_movie(void)
{
   vk_t vk;
   video_frame_import_t f;
   struct vulkan_filter_chain_texture input;
   memset(&vk, 0, sizeof(vk)); memset(&f, 0, sizeof(f));
   CHECK(vulkan_import_movie_frame(NULL, &f) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.dims = 4; outcome = VIDEO_TEXTURE_UPDATE_DONE;
   CHECK(vulkan_import_movie_frame(&vk, &f) == outcome && vk.movie_pending);
   vulkan_movie_input((struct vk_texture*)vk.movie_texture, &input);
   CHECK(input.image == 1 && input.view == 2 && input.layout == 3
         && input.dims == 4 && input.format == 5 && !input.origin && !input.padded_dims);
   CHECK(vulkan_movie_prepare(&vk, NULL) == NULL);
   CHECK(vulkan_movie_prepare(&vk, RETRO_HW_FRAME_BUFFER_VALID) == NULL
         && vk.movie_texture == (uintptr_t)&movie && !retired);
   vk.movie_pending = false; outcome = VIDEO_TEXTURE_UPDATE_DROPPED;
   CHECK(vulkan_import_movie_frame(&vk, &f) == outcome && !vk.movie_pending
         && vk.movie_texture == (uintptr_t)&movie && !retired);
   outcome = VIDEO_TEXTURE_UPDATE_REFUSED;
   CHECK(vulkan_import_movie_frame(&vk, &f) == outcome && !retired);
   vk.flags = VK_FLAG_HW_ENABLE; outcome = VIDEO_TEXTURE_UPDATE_DONE;
   CHECK(vulkan_import_movie_frame(&vk, &f) == VIDEO_TEXTURE_UPDATE_REFUSED);
   vk.flags = 0; f.dims = 8;
   CHECK(vulkan_import_movie_frame(&vk, &f) == outcome && retired == 1
         && movie.dims == 8 && vk.movie_pending);
   CHECK(vulkan_movie_prepare(&vk, &f) == &f && retired == 2
         && !vk.movie_texture && !vk.movie_pending);
   CHECK(vulkan_import_movie_frame(&vk, NULL) == outcome && retired == 2
         && !vk.movie_texture && !vk.movie_pending);
   CHECK(vulkan_import_movie_frame(&vk, NULL) == outcome && retired == 2);
}
static unsigned queued_imports, queued_clears, queued_abandons;
static enum video_texture_update queued_outcome;
static void queued_abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &queued_abandons && done == 88 && value == 7);
   queued_abandons++;
}
static enum video_texture_update queued_import(void *data, const video_frame_import_t *f)
{
   CHECK(data == &queued_imports);
   if (!f) { queued_clears++; return VIDEO_TEXTURE_UPDATE_DONE; }
   CHECK(f->image == 12 && f->wait == 34 && f->wait_value == 56);
   queued_imports++;
   return queued_outcome;
}
static void movie_queue(void)
{
   video_thread_movie_t next, slot;
   video_thread_movie_state_t state;
   video_frame_import_t f;
   video_poke_interface_t poke;
   video_thread_movie_state_init(&state);
   memset(&next, 0, sizeof(next)); memset(&slot, 0, sizeof(slot));
   memset(&f, 0, sizeof(f)); memset(&poke, 0, sizeof(poke));
   poke.import_movie_frame = queued_import;
   f.image = 12; f.wait = 34; f.wait_value = 56; f.done = 88; f.done_value = 7;
   f.owner = &queued_abandons; f.abandon = queued_abandon;
   CHECK(video_thread_movie_stage(&state, &next, &f) == VIDEO_TEXTURE_UPDATE_DONE);
   CHECK(video_thread_movie_stage(&state, &next, &f) == VIDEO_TEXTURE_UPDATE_DROPPED);
   CHECK(!queued_abandons);
   video_thread_movie_move(&slot, &next);
   CHECK(slot.pending && !next.pending && !queued_abandons);
   queued_outcome = VIDEO_TEXTURE_UPDATE_DONE;
   video_thread_movie_apply(&state, &slot, &queued_imports, &poke);
   CHECK(queued_imports == 1 && !queued_abandons && !slot.pending);
   video_thread_movie_stage(&state, &next, &f);
   video_thread_movie_move(&slot, &next);
   queued_outcome = VIDEO_TEXTURE_UPDATE_DROPPED;
   video_thread_movie_apply(&state, &slot, &queued_imports, &poke);
   CHECK(queued_abandons == 1 && !slot.pending);
   video_thread_movie_stage(&state, &next, &f);
   video_thread_movie_stage(&state, &next, NULL);
   CHECK(queued_abandons == 2 && next.reset && !next.pending);
   video_thread_movie_move(&slot, &next);
   video_thread_movie_stage(&state, &next, &f);
   video_thread_movie_move(&slot, &next);
   CHECK(slot.reset && slot.pending);
   queued_outcome = VIDEO_TEXTURE_UPDATE_REFUSED;
   video_thread_movie_apply(&state, &slot, &queued_imports, &poke);
   CHECK(queued_clears == 1 && queued_abandons == 3 && !slot.pending && !slot.reset);
   CHECK(!video_thread_movie_ready(&state));
   video_thread_movie_stage(&state, &next, NULL);
   video_thread_movie_stage(&state, &next, &f);
   video_thread_movie_move(&slot, &next);
   video_thread_movie_stage(&state, &next, &f);
   video_thread_movie_move(&slot, &next);
   CHECK(queued_abandons == 4);
   video_thread_movie_apply(&state, &slot, NULL, NULL);
   CHECK(queued_abandons == 5 && !video_thread_movie_ready(&state));
   video_thread_movie_stage(&state, &next, NULL);
   video_thread_movie_stage(&state, &next, &f);
   video_thread_movie_discard(&next);
   video_thread_movie_discard(&next);
   CHECK(queued_abandons == 6);
}
static void wrapper_failure(void)
{
   thread_video_t thr;
   video_thread_movie_t slot;
   video_poke_interface_t outer, inner;
   video_hw_frame_t hw;
   unsigned before;
   memset(&thr, 0, sizeof(thr)); memset(&slot, 0, sizeof(slot));
   memset(&hw, 0, sizeof(hw));
   memset(&outer, 0, sizeof(outer)); memset(&inner, 0, sizeof(inner));
   retro_atomic_int_init(&thr.movie_capable, 1);
   video_thread_movie_state_init(&thr.movie_state);
   outer.import_movie_frame = thread_import_movie_frame;
   inner.import_movie_frame = queued_import;
   thr.poke = &inner; thr.driver_data = &queued_imports;
   video_thread_thr_capture = &thr;
   video_driver_st.poke = &outer; video_driver_st.data = &thr;
   video_driver_st.movie_frame = video_driver_st.movie_refused = false;
   threaded = true;
   hw.image = 12; hw.wait = 34; hw.wait_value = 56;
   hw.done = 88; hw.done_value = 7;
   hw.owner = &queued_abandons; hw.abandon = queued_abandon;
   CHECK(video_driver_movie_refresh(NULL, NULL));
   CHECK(video_driver_movie_refresh(NULL, &hw));
   video_thread_movie_move(&slot, &thr.movie_next);
   video_driver_movie_clear();
   queued_outcome = VIDEO_TEXTURE_UPDATE_REFUSED;
   before = queued_abandons;
   video_thread_movie_apply(&thr.movie_state, &slot, thr.driver_data, thr.poke);
   CHECK(queued_abandons == before + 1);
   CHECK(video_driver_movie_refresh(NULL, NULL));
   CHECK(video_driver_movie_refresh(NULL, &hw));
   video_thread_movie_move(&slot, &thr.movie_next);
   queued_outcome = VIDEO_TEXTURE_UPDATE_DROPPED;
   video_thread_movie_apply(&thr.movie_state, &slot, thr.driver_data, thr.poke);
   CHECK(video_driver_movie_refresh(NULL, NULL));
   CHECK(video_driver_movie_refresh(NULL, &hw));
   video_thread_movie_move(&slot, &thr.movie_next);
   queued_outcome = VIDEO_TEXTURE_UPDATE_REFUSED;
   video_thread_movie_apply(&thr.movie_state, &slot, thr.driver_data, thr.poke);
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   before = queued_imports;
   CHECK(video_driver_movie_refresh(NULL, &hw));
   CHECK(video_driver_st.movie_refused && queued_imports == before);
   retro_atomic_store_release_int(&thr.movie_capable, 0);
   video_driver_movie_clear();
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   retro_atomic_store_release_int(&thr.movie_capable, 1);
   CHECK(video_driver_movie_refresh(NULL, NULL));
   CHECK(video_driver_movie_refresh(NULL, &hw));
   video_thread_movie_move(&slot, &thr.movie_next);
   queued_outcome = VIDEO_TEXTURE_UPDATE_DONE;
   video_thread_movie_apply(&thr.movie_state, &slot, thr.driver_data, thr.poke);
   queued_abandon(hw.owner, hw.done, hw.done_value);
   retro_atomic_store_release_size(&thr.movie_state.epoch, (size_t)-2);
   video_driver_movie_clear();
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   CHECK(!video_driver_st.movie_refused);
   CHECK(video_driver_movie_refresh(NULL, &hw));
   CHECK(video_driver_st.movie_refused);
   video_driver_movie_clear();
   CHECK(!video_driver_movie_refresh(NULL, NULL));
   video_thread_movie_state_init(&thr.movie_state);
   CHECK(video_driver_movie_refresh(NULL, NULL));
   video_thread_movie_discard(&thr.movie_next);
   threaded = false;
}

typedef struct
{
   thread_video_t thr;
   video_thread_movie_t slot;
   slock_t *lock;
   scond_t *cond;
   int entered, resume;
} movie_race_t;

static enum video_texture_update blocked_import(void *data,
      const video_frame_import_t *frame)
{
   movie_race_t *race = (movie_race_t*)data;
   if (!frame) return VIDEO_TEXTURE_UPDATE_DONE;
   CHECK(frame->image == 12);
   slock_lock(race->lock);
   race->entered = 1;
   scond_signal(race->cond);
   while (!race->resume)
      scond_wait(race->cond, race->lock);
   slock_unlock(race->lock);
   return VIDEO_TEXTURE_UPDATE_REFUSED;
}

static void delayed_refusal(void *data)
{
   movie_race_t *race = (movie_race_t*)data;
   video_thread_movie_apply(&race->thr.movie_state, &race->slot,
         race->thr.driver_data, race->thr.poke);
}

static void replacement_race(void)
{
   movie_race_t race;
   video_poke_interface_t outer, inner;
   video_hw_frame_t hw;
   unsigned i, before = queued_abandons;
   memset(&race, 0, sizeof(race)); memset(&hw, 0, sizeof(hw));
   memset(&outer, 0, sizeof(outer)); memset(&inner, 0, sizeof(inner));
   race.lock = slock_new(); race.cond = scond_new();
   CHECK(race.lock && race.cond);
   if (!race.lock || !race.cond) return;
   retro_atomic_int_init(&race.thr.movie_capable, 1);
   video_thread_movie_state_init(&race.thr.movie_state);
   inner.import_movie_frame = blocked_import;
   outer.import_movie_frame = thread_import_movie_frame;
   race.thr.poke = &inner; race.thr.driver_data = &race;
   video_thread_thr_capture = &race.thr;
   video_driver_st.poke = &outer; video_driver_st.data = &race.thr;
   video_driver_st.movie_refused = video_driver_st.movie_frame = false;
   threaded = true;
   hw.image = 12; hw.done = 88; hw.done_value = 7;
   hw.owner = &queued_abandons; hw.abandon = queued_abandon;
   for (i = 0; i < 100; i++)
   {
      sthread_t *worker;
      race.entered = race.resume = 0;
      CHECK(video_driver_movie_refresh(NULL, &hw));
      video_thread_movie_move(&race.slot, &race.thr.movie_next);
      worker = sthread_create(delayed_refusal, &race);
      CHECK(worker != NULL);
      if (!worker) break;
      slock_lock(race.lock);
      while (!race.entered)
         scond_wait(race.cond, race.lock);
      video_driver_movie_clear();
      CHECK(video_driver_movie_refresh(NULL, NULL));
      race.resume = 1;
      scond_signal(race.cond);
      slock_unlock(race.lock);
      sthread_join(worker);
      CHECK(video_driver_movie_refresh(NULL, NULL));
   }
   CHECK(i == 100 && queued_abandons == before + 100);
   video_thread_movie_discard(&race.thr.movie_next);
   slock_free(race.lock); scond_free(race.cond);
   threaded = false;
   printf("Movie refusal: 100 delayed worker results preserve replacement readiness\n");
}

int main(void)
{
   frontend(); drivers(); vulkan_movie(); movie_queue(); wrapper_failure(); replacement_race(); printf("Frontend movie GPU handoff: %u failures\n", failures); return failures ? 1 : 0;
}
