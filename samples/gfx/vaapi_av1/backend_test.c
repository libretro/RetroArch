/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_av1_backend.h"
#include <rthreads/retro_eventcount.h>
#include <formats/image_yuv_blit.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct vaapi_av1_owner
{
   bool detached;
};

static vaapi_av1_backend_t backend;
static rav1_sequence_t params;
static video_color_metadata_t coded;
static const rav1_hw_backend_t *registered;
static retro_atomic_int_t gate, polls, destroyed, opened, entered, hold;
static pthread_t caller;
static int path_fail, native_fail, allocation_fail;
static int event_fail, attr_fail, detach_fail, thread_fail, notify_race;
static unsigned paths;
static video_color_metadata_t seen_color;
static unsigned seen_major, seen_minor;

void rav1_set_hw_backend(const rav1_hw_backend_t *offer)
{
   registered = offer;
}

int rav1_unset_hw_backend(const rav1_hw_backend_t *offer)
{
   if (registered != offer)
      return 0;
   registered = NULL;
   return 1;
}

static bool test_node(
      unsigned major_id, unsigned minor_id, char *path, size_t capacity)
{
   assert(capacity >= sizeof("/dev/dri/renderD128"));
   seen_major = major_id;
   seen_minor = minor_id;
   paths++;
   memcpy(path, "/dev/dri/renderD128", sizeof("/dev/dri/renderD128"));
   return !path_fail;
}

static int test_decode(void *owner, const uint8_t *data, size_t size,
      const rav1_sequence_t *sequence, const rav1_hw_picture_t *picture)
{
   assert(owner && !data && !size && !sequence && !picture);
   return 7;
}

static int test_output(void *owner, unsigned slot, const rav1_grain_t *grain,
      video_hw_frame_t *frame)
{
   assert(owner && slot == 2 && !grain && !frame);
   return 8;
}

bool vaapi_av1_owner_open(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *p,
      const video_color_metadata_t *color, unsigned slots, rav1_hw_ops_t *ops)
{
   assert(!*out && !ops->decoder && p == &params && slots == 32
         && !strcmp(path, "/dev/dri/renderD128") && major_id == seen_major
         && minor_id == seen_minor);
   seen_color = *color;
   retro_atomic_fetch_add_int(&opened, 1);
   retro_atomic_store_release_int(&entered, 1);
   while (retro_atomic_load_acquire_int(&hold))
      usleep(1000);
   if (native_fail == 1)
      return false;
   *out = (vaapi_av1_owner_t *)malloc(sizeof(**out));
   assert(*out);
   (*out)->detached = native_fail == 2;
   if (native_fail == 2)
      return false;
   ops->decoder = *out;
   ops->decode = test_decode;
   ops->output = test_output;
   return true;
}

vaapi_av1_owner_t *vaapi_av1_owner_detach(rav1_hw_ops_t *ops)
{
   vaapi_av1_owner_t *owner = (vaapi_av1_owner_t *)ops->decoder;
   assert(owner && !owner->detached);
   owner->detached = true;
   memset(ops, 0, sizeof(*ops));
   return owner;
}

bool vaapi_av1_owner_retire(vaapi_av1_owner_t *owner)
{
   assert(!pthread_equal(pthread_self(), caller));
   if (!owner)
      return true;
   assert(owner->detached);
   retro_atomic_fetch_add_int(&polls, 1);
   if (!retro_atomic_load_acquire_int(&gate))
      return false;
   free(owner);
   retro_atomic_fetch_add_int(&destroyed, 1);
   return true;
}

static void *test_calloc(size_t count, size_t size)
{
   return allocation_fail ? NULL : calloc(count, size);
}

static bool test_event_init(retro_eventcount_t *wake)
{
   return !event_fail && retro_eventcount_init(wake);
}

static int test_attr_init(pthread_attr_t *attr)
{
   return attr_fail ? ENOMEM : pthread_attr_init(attr);
}

static int test_detach(pthread_attr_t *attr, int state)
{
   return detach_fail ? EINVAL : pthread_attr_setdetachstate(attr, state);
}

static int test_thread(pthread_t *thread, const pthread_attr_t *attr,
      void *(*run)(void *), void *opaque)
{
   int state;
   assert(!pthread_attr_getdetachstate(attr, &state)
         && state == PTHREAD_CREATE_DETACHED);
   return thread_fail ? EAGAIN : pthread_create(thread, attr, run, opaque);
}

static void test_notify(retro_eventcount_t *wake);
#define calloc test_calloc
#define VAAPI_AV1_NODE_PATH test_node
#define retro_eventcount_init test_event_init
#define retro_eventcount_notify test_notify
#define pthread_attr_init test_attr_init
#define pthread_attr_setdetachstate test_detach
#define pthread_create test_thread
#include "../../../gfx/common/vaapi_av1_backend.c"
#undef pthread_create
#undef pthread_attr_setdetachstate
#undef pthread_attr_init
#undef retro_eventcount_notify
#undef retro_eventcount_init
#undef calloc

#include "../../../gfx/common/vulkan_drm_identity.h"

typedef struct test_context
{
   vulkan_drm_identity_t drm_identity;
   VkDevice device;
   unsigned video_codec_operations;
   bool prime_import_device;
} test_context_t;

typedef struct test_renderer
{
   test_context_t *context;
} vk_t;

#include "../../../gfx/drivers/vulkan_vaapi_av1_backend.h"

static void test_driver_admission(void)
{
   test_context_t context;
   rav1_hw_backend_t native;
   vk_t renderer, replacement;
   memset(&context, 0, sizeof(context));
   renderer.context = &context;
   replacement.context = &context;
   assert(!vulkan_vaapi_av1_decoder_publish(&renderer, false));
   context.device = (VkDevice)(uintptr_t)1;
   context.prime_import_device = true;
   assert(!vulkan_vaapi_av1_decoder_publish(&renderer, false));
   context.drm_identity.valid = true;
   context.drm_identity.major_id = 226;
   context.drm_identity.minor_id = 128;
   context.video_codec_operations = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
   assert(!vulkan_vaapi_av1_decoder_publish(&renderer, true));
   assert(vulkan_vaapi_av1_decoder_publish(&renderer, false));
   assert(registered == &vulkan_vaapi_av1_backend.offer);
   assert(vulkan_vaapi_av1_backend.max_sessions == 8);
   memset(&native, 0, sizeof(native));
   registered = &native;
   assert(!vulkan_vaapi_av1_decoder_publish(&renderer, true));
   assert(registered == &native);
   context.video_codec_operations = 0;
   assert(vulkan_vaapi_av1_decoder_publish(&replacement, true));
   assert(!vaapi_av1_backend_withdraw(&vulkan_vaapi_av1_backend, &renderer));
   context.prime_import_device = false;
   assert(
         !vulkan_vaapi_av1_decoder_publish(&replacement, false) && !registered);
   assert(!retro_atomic_load_acquire_int(&vulkan_vaapi_av1_backend.active));
}

static void test_notify(retro_eventcount_t *wake)
{
   vaapi_av1_backend_session_t *session;
   unsigned i;
   retro_eventcount_notify(wake);
   if (!notify_race)
      return;
   session = (vaapi_av1_backend_session_t *)((char *)wake
         - offsetof(vaapi_av1_backend_session_t, wake));
   for (i = 0; i < 2000 && retro_atomic_load_acquire_int(&session->refs) != 1;
         i++)
      usleep(1000);
   assert(retro_atomic_load_acquire_int(&session->refs) == 1);
}

static void wait_value(retro_atomic_int_t *value, int expected)
{
   unsigned i;
   for (i = 0; i < 2000 && retro_atomic_load_acquire_int(value) != expected;
         i++)
      usleep(1000);
   assert(retro_atomic_load_acquire_int(value) == expected);
}

static void wait_polls(void)
{
   unsigned i;
   for (i = 0; i < 2000 && retro_atomic_load_acquire_int(&polls) < 2; i++)
      usleep(1000);
   assert(retro_atomic_load_acquire_int(&polls) >= 2);
}

static bool test_open(rav1_hw_ops_t *ops)
{
   return backend.offer.open_config(
         backend.offer.user, &params, NULL, &coded, 9, ops);
}

static void *setup_thread(void *opaque)
{
   rav1_hw_ops_t ops;
   memset(&ops, 0, sizeof(ops));
   assert(!test_open(&ops) && !ops.decoder);
   (void)opaque;
   return NULL;
}

int main(void)
{
   rav1_hw_ops_t first, second, refused;
   rav1_hw_backend_t foreign;
   vaapi_av1_backend_t exhausted;
   pthread_t setup;
   unsigned i, before;
   int owners[2];
   caller = pthread_self();
   test_driver_admission();
   retro_atomic_int_init(&gate, 0);
   retro_atomic_int_init(&polls, 0);
   retro_atomic_int_init(&destroyed, 0);
   retro_atomic_int_init(&opened, 0);
   retro_atomic_int_init(&entered, 0);
   retro_atomic_int_init(&hold, 0);
   memset(&first, 0, sizeof(first));
   memset(&second, 0, sizeof(second));
   memset(&refused, 0, sizeof(refused));
   memset(&foreign, 0, sizeof(foreign));
   memset(&params, 0, sizeof(params));
   params.max_width = params.max_height = 64;
   params.operating_point_count = 1;
   params.film_grain_params_present = 1;
   params.bit_depth = 8;
   params.subsampling_x = params.subsampling_y = 1;
   params.color_primaries = params.matrix_coefficients = 1;
   params.transfer_characteristics = 1;
   coded.primaries = coded.transfer = coded.matrix = 1;
   assert(!vaapi_av1_backend_init(&backend, 0));
   assert(!vaapi_av1_backend_init(&backend, 9));
   assert(vaapi_av1_backend_init(&backend, 2));
   assert(!registered && !test_open(&refused));
   assert(vaapi_av1_backend_publish(&backend, owners, 226, 128));
   assert(registered == &backend.offer);
   for (i = 0; i < 7; i++)
   {
      path_fail = i == 0;
      allocation_fail = i == 1;
      event_fail = i == 2;
      attr_fail = i == 3;
      detach_fail = i == 4;
      thread_fail = i == 5;
      params.subsampling_x = i == 6 ? 0 : 1;
      assert(!test_open(&refused) && !refused.decoder);
      assert(!retro_atomic_load_acquire_int(&backend.active));
   }
   path_fail = allocation_fail = event_fail = attr_fail = detach_fail = 0;
   thread_fail = 0;
   params.subsampling_x = 1;
   assert(test_open(&first) && test_open(&second));
   assert(seen_color.matrix == 1 && seen_color.range == 1);
   before = paths;
   assert(!test_open(&refused) && paths == before);
   assert(first.decode(first.decoder, NULL, 0, NULL, NULL) == 7);
   assert(first.output(first.decoder, 2, NULL, NULL) == 8);
   backend.offer.close(&first);
   backend.offer.close(&second);
   assert(!first.decoder && !second.decoder);
   wait_polls();
   assert(!retro_atomic_load_acquire_int(&destroyed));
   assert(!test_open(&refused) && paths == before);
   retro_atomic_store_release_int(&gate, 1);
   wait_value(&backend.active, 0);
   assert(retro_atomic_load_acquire_int(&destroyed) == 2);
   notify_race = 1;
   for (native_fail = 1; native_fail <= 2; native_fail++)
   {
      assert(!test_open(&refused) && !refused.decoder);
      wait_value(&backend.active, 0);
   }
   native_fail = 0;
   params.color_primaries = params.matrix_coefficients = 9;
   params.transfer_characteristics = 2;
   coded.primaries = coded.matrix = 9;
   coded.transfer = 2;
   assert(test_open(&first));
   assert(seen_color.transfer == 2 && !seen_color.max_cll);
   backend.offer.close(&first);
   wait_value(&backend.active, 0);
   params.bit_depth = 10;
   params.profile = 0;
   params.color_range = 1;
   coded.range = 2;
   params.color_primaries = params.matrix_coefficients = 9;
   params.transfer_characteristics = 16;
   coded.primaries = coded.matrix = 9;
   coded.transfer = 16;
   assert(test_open(&first));
   assert(seen_color.transfer == 16 && seen_color.range == 2);
   assert(!vaapi_av1_backend_withdraw(&backend, owners + 1));
   assert(vaapi_av1_backend_withdraw(&backend, owners));
   assert(!registered && !test_open(&refused));
   backend.offer.close(&first);
   wait_value(&backend.active, 0);
   assert(vaapi_av1_backend_publish(&backend, owners, 226, 129));
   notify_race = 0;
   retro_atomic_store_release_int(&entered, 0);
   retro_atomic_store_release_int(&hold, 1);
   assert(!pthread_create(&setup, NULL, setup_thread, NULL));
   wait_value(&entered, 1);
   assert(vaapi_av1_backend_publish(&backend, owners + 1, 227, 130));
   assert(!vaapi_av1_backend_withdraw(&backend, owners));
   retro_atomic_store_release_int(&hold, 0);
   assert(!pthread_join(setup, NULL));
   wait_value(&backend.active, 0);
   assert(test_open(&first) && seen_major == 227 && seen_minor == 130);
   backend.offer.close(&first);
   wait_value(&backend.active, 0);
   for (params.chroma_sample_position = 1; params.chroma_sample_position <= 2;
         params.chroma_sample_position++)
   {
      assert(test_open(&first));
      backend.offer.close(&first);
      wait_value(&backend.active, 0);
   }
   before = paths;
   assert(!test_open(&refused) && paths == before);
   params.chroma_sample_position = 0;
   assert(!backend.offer.open_config(
                backend.offer.user, &params, NULL, &coded, 8, &refused)
         && paths == before);
   assert(backend.offer.open_config(
         backend.offer.user, &params, NULL, &coded, 9, &first));
   assert(seen_color.transfer == 16 && seen_color.range == 2);
   backend.offer.close(&first);
   wait_value(&backend.active, 0);
   registered = &foreign;
   assert(vaapi_av1_backend_withdraw(&backend, owners + 1));
   assert(registered == &foreign);
   assert(vaapi_av1_backend_init(&exhausted, 2));
   retro_atomic_store_release_int(&exhausted.epoch, INT_MAX - 2);
   assert(!vaapi_av1_backend_publish(&exhausted, owners, 226, 128));
   assert(!vaapi_av1_backend_publish(&exhausted, owners, 226, 128));
   assert(retro_atomic_load_acquire_int(&exhausted.epoch) == -1);
   puts("PASS: VA factory admission, bounded cold retirement, startup "
        "failures, held ownership, notification lifetime and renderer-epoch "
        "replacement; no native VA decode");
   return 0;
}
