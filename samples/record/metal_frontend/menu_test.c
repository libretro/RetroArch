/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define RECORD_MEDIACODEC_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../configuration.h"
#include "../../../config.def.h"
#include "../../../record/record_native_buffer.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../record/record_driver.h"
#include "../../../record/drivers/record_metal.h"
#include "../../../record/drivers/record_nvenc.h"
#include "../../../record/drivers/record_mediacodec.h"

static rarch_setting_t buffer_setting;
static settings_t test_settings;
static struct menu_state test_menu;
static bool metal_available = true, nvenc_available = true;
static bool mediacodec_available;
static enum msg_hash_enums rows[32];
static unsigned row_count;
static const unsigned native_modes[] = {
   STREAMING_MODE_TWITCH, STREAMING_MODE_YOUTUBE,
#ifdef HAVE_SSL
   STREAMING_MODE_FACEBOOK, STREAMING_MODE_KICK,
#endif
   STREAMING_MODE_CUSTOM
};

settings_t *config_get_ptr(void)
{
   return &test_settings;
}
struct menu_state *menu_state_get_ptr(void)
{
   return &test_menu;
}
static void *dummy_init(const struct record_params *params)
{
   (void)params;
   return NULL;
}
const record_driver_t record_metal = {dummy_init,NULL,NULL,NULL,NULL,"metal"};
const record_driver_t record_nvenc = {dummy_init,NULL,NULL,NULL,NULL,"nvenc"};
const record_driver_t record_mediacodec = {dummy_init,NULL,NULL,NULL,NULL,"mediacodec"};
static const record_driver_t ffmpeg = {dummy_init,NULL,NULL,NULL,NULL,"ffmpeg"};
static const record_driver_t avf = {dummy_init,NULL,NULL,NULL,NULL,"avfoundation"};
static const record_driver_t wav = {dummy_init,NULL,NULL,NULL,NULL,"wav"};
const record_driver_t *record_drivers[] = {
   &record_metal,&record_nvenc,&record_mediacodec,&ffmpeg,&avf,&wav,NULL
};
bool record_metal_can_stream(void)
{
   return metal_available;
}
bool record_nvenc_can_stream(void)
{
   return nvenc_available;
}
bool record_mediacodec_can_stream(void)
{
   return mediacodec_available;
}
rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums label)
{
   assert(label == MENU_ENUM_LABEL_VIDEO_RECORD_FIFO_FRAMES);
   return &buffer_setting;
}
static int add_row(enum msg_hash_enums label)
{
   assert(row_count < sizeof(rows)/sizeof(rows[0]));
   rows[row_count++] = label;
   return 0;
}
static bool has_row(enum msg_hash_enums label)
{
   unsigned i;
   for (i = 0; i < row_count; i++)
      if (rows[i] == label)
         return true;
   return false;
}
static float recalc_step_based_on_length_of_action(rarch_setting_t *setting)
{
   (void)setting;
   return 1.0f;
}
#undef MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM
#define MENU_DISPLAYLIST_PARSE_SETTINGS_ENUM(list,label,type,empty) \
   ((void)(list),(void)(type),(void)(empty),add_row(label))
#include "menu_fragment.c"

static void select_driver(const char *ident)
{
   assert(strlen(ident) < sizeof(test_settings.arrays.record_driver));
   strcpy(test_settings.arrays.record_driver,ident);
   assert(build_rows() == row_count);
   setting_record_buffer_range(&buffer_setting);
}
static void check_native(const char *ident, bool streaming)
{
   unsigned offered[6];
   select_driver(ident);
   test_settings.bools.quick_menu_show_start_streaming = true;
   assert(quick_stream_offered() == streaming);
   assert(dropdown_modes(offered) == (streaming ? ARRAY_SIZE(native_modes) : 0u));
   if (streaming)
      assert(!memcmp(offered,native_modes,sizeof(native_modes)));
   assert(has_row(MENU_ENUM_LABEL_RECORD_CONFIG));
   assert(has_row(MENU_ENUM_LABEL_VIDEO_GPU_RECORD));
   assert(has_row(MENU_ENUM_LABEL_VIDEO_RECORD_ALLOW_FRAME_DROP));
   assert(has_row(MENU_ENUM_LABEL_VIDEO_RECORD_FIFO_FRAMES));
   assert(buffer_setting.min == recording_native_buffer_min(ident));
   assert(buffer_setting.max == RECORD_GPU_QUEUE_SLOTS);
   assert(buffer_setting.step == 1 && buffer_setting.default_value.unsigned_integer == 4);
   assert(setting_uint_get(&buffer_setting) >= buffer_setting.min);
   assert(setting_uint_get(&buffer_setting) <= buffer_setting.max);
   assert(!has_row(MENU_ENUM_LABEL_VIDEO_RECORD_THREADS));
   assert(!has_row(MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY));
   assert(!has_row(MENU_ENUM_LABEL_VIDEO_RECORD_GAME_ONLY));
   assert(!has_row(MENU_ENUM_LABEL_VIDEO_RECORD_HW_ENCODER));
   assert(!has_row(MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD));
   assert(has_row(MENU_ENUM_LABEL_STREAMING_MODE) == streaming);
   assert(has_row(MENU_ENUM_LABEL_STREAMING_URL) == streaming);
   assert(has_row(MENU_ENUM_LABEL_STREAM_CONFIG) == streaming);
   assert(!has_row(MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY));
   assert(!has_row(MENU_ENUM_LABEL_UDP_STREAM_PORT));
   assert(!recording_driver_streaming_mode_supported(ident,STREAMING_MODE_LOCAL));
   assert(!recording_driver_streaming_mode_supported(ident,STREAMING_MODE_CUSTOM+1u));
}
int main(void)
{
   rarch_setting_t setting;
   unsigned mode, offered[6], slots;
   for (slots = 0; slots <= RECORD_GPU_QUEUE_SLOTS + 1; slots++)
   {
      assert(recording_native_buffer_count("metal",slots)
            == (slots >= 2 && slots <= 16 ? slots : 4));
      assert(recording_native_buffer_count("mediacodec",slots)
            == (slots >= 2 && slots <= 16 ? slots : 4));
      assert(recording_native_buffer_count("nvenc",slots)
            == (slots >= 4 && slots <= 16 ? slots : 4));
   }
   memset(&buffer_setting,0,sizeof(buffer_setting));
   buffer_setting.value.target.unsigned_integer = &test_settings.uints.video_record_fifo_frames;
   test_settings.uints.video_record_fifo_frames = DEFAULT_VIDEO_RECORD_FIFO_FRAMES;
   assert(!recording_driver_find(NULL) && !recording_driver_find("missing"));
   assert(!recording_driver_can_stream("missing"));
   check_native("metal",true);
   check_native("nvenc",true);
   check_native("mediacodec",false);
   mediacodec_available = true;
   check_native("mediacodec",true);
   mediacodec_available = false;
   metal_available = nvenc_available = false;
   check_native("metal",false);
   check_native("nvenc",false);
   metal_available = nvenc_available = true;
   select_driver("wav");
   assert(row_count == 1 && has_row(MENU_ENUM_LABEL_RECORD_DRIVER));
   select_driver("avfoundation");
   assert(has_row(MENU_ENUM_LABEL_VIDEO_RECORD_QUALITY));
   assert(!has_row(MENU_ENUM_LABEL_STREAMING_URL));
   test_settings.uints.streaming_mode = STREAMING_MODE_LOCAL;
   select_driver("ffmpeg");
   assert(buffer_setting.min == MIN_VIDEO_RECORD_FIFO_FRAMES);
   assert(buffer_setting.max == MAX_VIDEO_RECORD_FIFO_FRAMES);
   assert(buffer_setting.step == 4);
   assert(buffer_setting.default_value.unsigned_integer == DEFAULT_VIDEO_RECORD_FIFO_FRAMES);
   assert(has_row(MENU_ENUM_LABEL_UDP_STREAM_PORT));
   assert(has_row(MENU_ENUM_LABEL_VIDEO_STREAM_QUALITY));
   assert(has_row(MENU_ENUM_LABEL_STREAMING_TITLE));
   assert(quick_stream_offered() && dropdown_modes(offered) == 6);
   assert(offered[4] == STREAMING_MODE_LOCAL);
   test_settings.bools.quick_menu_show_start_streaming = false;
   assert(!quick_stream_offered());
   for (mode = 0; mode <= STREAMING_MODE_CUSTOM; mode++)
      assert(recording_driver_streaming_mode_supported("ffmpeg",mode));

   memset(&setting,0,sizeof(setting));
   setting.value.target.unsigned_integer = &test_settings.uints.streaming_mode;
   setting.min = 0;
   setting.max = STREAMING_MODE_CUSTOM;
   setting.flags = SD_FLAG_ENFORCE_MINRANGE | SD_FLAG_ENFORCE_MAXRANGE;
   test_settings.bools.menu_navigation_wraparound_enable = true;
   select_driver("metal");
   test_settings.uints.streaming_mode = STREAMING_MODE_CUSTOM;
   assert(!setting_uint_action_left_streaming_mode(&setting,0,true));
   assert(test_settings.uints.streaming_mode == native_modes[ARRAY_SIZE(native_modes)-2]);
   assert(!setting_uint_action_right_streaming_mode(&setting,0,true));
   assert(test_settings.uints.streaming_mode == STREAMING_MODE_CUSTOM);
   assert(!setting_uint_action_right_streaming_mode(&setting,0,true));
   assert(test_settings.uints.streaming_mode == STREAMING_MODE_TWITCH);
   assert(!setting_uint_action_left_streaming_mode(&setting,0,true));
   assert(test_settings.uints.streaming_mode == STREAMING_MODE_CUSTOM);
   test_settings.uints.streaming_mode = STREAMING_MODE_LOCAL;
   record_driver_write_handler(&setting);
   assert(test_settings.uints.streaming_mode == STREAMING_MODE_CUSTOM);
   assert(test_menu.flags & MENU_ST_FLAG_ENTRIES_NEED_REFRESH);
   select_driver("ffmpeg");
   assert(!setting_uint_action_left_streaming_mode(&setting,0,true));
   assert(test_settings.uints.streaming_mode == STREAMING_MODE_LOCAL);
   assert(!setting_uint_action_right_streaming_mode(&setting,0,true));
   assert(test_settings.uints.streaming_mode == STREAMING_MODE_CUSTOM);
   assert(setting_uint_action_left_streaming_mode(NULL,0,true) == -1);
   assert(setting_uint_action_right_streaming_mode(NULL,0,true) == -1);
   puts("PASS actual recording settings rows, build capabilities and streaming mode navigation");
   return 0;
}
