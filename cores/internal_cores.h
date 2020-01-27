/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INTERNAL_CORES_H__
#define INTERNAL_CORES_H__

#include <boolean.h>
#include <libretro.h>
#include <retro_common_api.h>
#include <retro_environment.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

RETRO_BEGIN_DECLS

void libretro_dummy_retro_init(void);

void libretro_dummy_retro_deinit(void);

unsigned libretro_dummy_retro_api_version(void);

void libretro_dummy_retro_get_system_info(struct retro_system_info *info);

void libretro_dummy_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_dummy_retro_set_environment(retro_environment_t cb);

void libretro_dummy_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_dummy_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_dummy_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_dummy_retro_set_input_poll(retro_input_poll_t cb);

void libretro_dummy_retro_set_input_state(retro_input_state_t cb);

void libretro_dummy_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_dummy_retro_reset(void);

void libretro_dummy_retro_run(void);

size_t libretro_dummy_retro_serialize_size(void);

bool libretro_dummy_retro_serialize(void *data, size_t size);

bool libretro_dummy_retro_unserialize(const void *data, size_t size);

void libretro_dummy_retro_cheat_reset(void);

void libretro_dummy_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_dummy_retro_load_game(const struct retro_game_info *game);

bool libretro_dummy_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_dummy_retro_unload_game(void);

unsigned libretro_dummy_retro_get_region(void);

void *libretro_dummy_retro_get_memory_data(unsigned id);

size_t libretro_dummy_retro_get_memory_size(unsigned id);

#ifdef HAVE_FFMPEG
/* Internal ffmpeg core. */

void libretro_ffmpeg_retro_init(void);

void libretro_ffmpeg_retro_deinit(void);

unsigned libretro_ffmpeg_retro_api_version(void);

void libretro_ffmpeg_retro_get_system_info(struct retro_system_info *info);

void libretro_ffmpeg_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_ffmpeg_retro_set_environment(retro_environment_t cb);

void libretro_ffmpeg_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_ffmpeg_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_ffmpeg_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_ffmpeg_retro_set_input_poll(retro_input_poll_t cb);

void libretro_ffmpeg_retro_set_input_state(retro_input_state_t cb);

void libretro_ffmpeg_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_ffmpeg_retro_reset(void);

void libretro_ffmpeg_retro_run(void);

size_t libretro_ffmpeg_retro_serialize_size(void);

bool libretro_ffmpeg_retro_serialize(void *data, size_t size);

bool libretro_ffmpeg_retro_unserialize(const void *data, size_t size);

void libretro_ffmpeg_retro_cheat_reset(void);

void libretro_ffmpeg_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_ffmpeg_retro_load_game(const struct retro_game_info *game);

bool libretro_ffmpeg_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_ffmpeg_retro_unload_game(void);

unsigned libretro_ffmpeg_retro_get_region(void);

void *libretro_ffmpeg_retro_get_memory_data(unsigned id);

size_t libretro_ffmpeg_retro_get_memory_size(unsigned id);

#endif

#ifdef HAVE_MPV
/* Internal mpv core. */

void libretro_mpv_retro_init(void);

void libretro_mpv_retro_deinit(void);

unsigned libretro_mpv_retro_api_version(void);

void libretro_mpv_retro_get_system_info(struct retro_system_info *info);

void libretro_mpv_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_mpv_retro_set_environment(retro_environment_t cb);

void libretro_mpv_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_mpv_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_mpv_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_mpv_retro_set_input_poll(retro_input_poll_t cb);

void libretro_mpv_retro_set_input_state(retro_input_state_t cb);

void libretro_mpv_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_mpv_retro_reset(void);

void libretro_mpv_retro_run(void);

size_t libretro_mpv_retro_serialize_size(void);

bool libretro_mpv_retro_serialize(void *data, size_t size);

bool libretro_mpv_retro_unserialize(const void *data, size_t size);

void libretro_mpv_retro_cheat_reset(void);

void libretro_mpv_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_mpv_retro_load_game(const struct retro_game_info *game);

bool libretro_mpv_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_mpv_retro_unload_game(void);

unsigned libretro_mpv_retro_get_region(void);

void *libretro_mpv_retro_get_memory_data(unsigned id);

size_t libretro_mpv_retro_get_memory_size(unsigned id);

#endif

#ifdef HAVE_IMAGEVIEWER
/* Internal image viewer core. */

void libretro_imageviewer_retro_init(void);

void libretro_imageviewer_retro_deinit(void);

unsigned libretro_imageviewer_retro_api_version(void);

void libretro_imageviewer_retro_get_system_info(struct retro_system_info *info);

void libretro_imageviewer_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_imageviewer_retro_set_environment(retro_environment_t cb);

void libretro_imageviewer_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_imageviewer_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_imageviewer_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_imageviewer_retro_set_input_poll(retro_input_poll_t cb);

void libretro_imageviewer_retro_set_input_state(retro_input_state_t cb);

void libretro_imageviewer_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_imageviewer_retro_reset(void);

void libretro_imageviewer_retro_run(void);

size_t libretro_imageviewer_retro_serialize_size(void);

bool libretro_imageviewer_retro_serialize(void *data, size_t size);

bool libretro_imageviewer_retro_unserialize(const void *data, size_t size);

void libretro_imageviewer_retro_cheat_reset(void);

void libretro_imageviewer_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_imageviewer_retro_load_game(const struct retro_game_info *game);

bool libretro_imageviewer_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_imageviewer_retro_unload_game(void);

unsigned libretro_imageviewer_retro_get_region(void);

void *libretro_imageviewer_retro_get_memory_data(unsigned id);

size_t libretro_imageviewer_retro_get_memory_size(unsigned id);

#endif

#if defined(HAVE_NETWORKGAMEPAD) && defined(HAVE_NETWORKING)
/* Internal networked retropad core. */

void libretro_netretropad_retro_init(void);

void libretro_netretropad_retro_deinit(void);

unsigned libretro_netretropad_retro_api_version(void);

void libretro_netretropad_retro_get_system_info(struct retro_system_info *info);

void libretro_netretropad_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_netretropad_retro_set_environment(retro_environment_t cb);

void libretro_netretropad_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_netretropad_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_netretropad_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_netretropad_retro_set_input_poll(retro_input_poll_t cb);

void libretro_netretropad_retro_set_input_state(retro_input_state_t cb);

void libretro_netretropad_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_netretropad_retro_reset(void);

void libretro_netretropad_retro_run(void);

size_t libretro_netretropad_retro_serialize_size(void);

bool libretro_netretropad_retro_serialize(void *data, size_t size);

bool libretro_netretropad_retro_unserialize(const void *data, size_t size);

void libretro_netretropad_retro_cheat_reset(void);

void libretro_netretropad_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_netretropad_retro_load_game(const struct retro_game_info *game);

bool libretro_netretropad_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_netretropad_retro_unload_game(void);

unsigned libretro_netretropad_retro_get_region(void);

void *libretro_netretropad_retro_get_memory_data(unsigned id);

size_t libretro_netretropad_retro_get_memory_size(unsigned id);

#endif

#if defined(HAVE_V4L2)
/* Internal video processor core. */

void libretro_videoprocessor_retro_init(void);

void libretro_videoprocessor_retro_deinit(void);

unsigned libretro_videoprocessor_retro_api_version(void);

void libretro_videoprocessor_retro_get_system_info(struct retro_system_info *info);

void libretro_videoprocessor_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_videoprocessor_retro_set_environment(retro_environment_t cb);

void libretro_videoprocessor_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_videoprocessor_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_videoprocessor_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_videoprocessor_retro_set_input_poll(retro_input_poll_t cb);

void libretro_videoprocessor_retro_set_input_state(retro_input_state_t cb);

void libretro_videoprocessor_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_videoprocessor_retro_reset(void);

void libretro_videoprocessor_retro_run(void);

size_t libretro_videoprocessor_retro_serialize_size(void);

bool libretro_videoprocessor_retro_serialize(void *data, size_t size);

bool libretro_videoprocessor_retro_unserialize(const void *data, size_t size);

void libretro_videoprocessor_retro_cheat_reset(void);

void libretro_videoprocessor_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_videoprocessor_retro_load_game(const struct retro_game_info *game);

bool libretro_videoprocessor_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_videoprocessor_retro_unload_game(void);

unsigned libretro_videoprocessor_retro_get_region(void);

void *libretro_videoprocessor_retro_get_memory_data(unsigned id);

size_t libretro_videoprocessor_retro_get_memory_size(unsigned id);

#endif

#ifdef HAVE_GONG
/* Internal gong core. */

void libretro_gong_retro_init(void);

void libretro_gong_retro_deinit(void);

unsigned libretro_gong_retro_api_version(void);

void libretro_gong_retro_get_system_info(struct retro_system_info *info);

void libretro_gong_retro_get_system_av_info(struct retro_system_av_info *info);

void libretro_gong_retro_set_environment(retro_environment_t cb);

void libretro_gong_retro_set_video_refresh(retro_video_refresh_t cb);

void libretro_gong_retro_set_audio_sample(retro_audio_sample_t cb);

void libretro_gong_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

void libretro_gong_retro_set_input_poll(retro_input_poll_t cb);

void libretro_gong_retro_set_input_state(retro_input_state_t cb);

void libretro_gong_retro_set_controller_port_device(unsigned port, unsigned device);

void libretro_gong_retro_reset(void);

void libretro_gong_retro_run(void);

size_t libretro_gong_retro_serialize_size(void);

bool libretro_gong_retro_serialize(void *data, size_t size);

bool libretro_gong_retro_unserialize(const void *data, size_t size);

void libretro_gong_retro_cheat_reset(void);

void libretro_gong_retro_cheat_set(unsigned index, bool enabled, const char *code);

bool libretro_gong_retro_load_game(const struct retro_game_info *game);

bool libretro_gong_retro_load_game_special(unsigned game_type,
      const struct retro_game_info *info, size_t num_info);

void libretro_gong_retro_unload_game(void);

unsigned libretro_gong_retro_get_region(void);

void *libretro_gong_retro_get_memory_data(unsigned id);

size_t libretro_gong_retro_get_memory_size(unsigned id);

#endif

RETRO_END_DECLS

#endif
