/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Higor Euripedes
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#ifndef TASKS_HANDLER_CONTENT_H
#define TASKS_HANDLER_CONTENT_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include <queues/task_queue.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../content.h"
#include "../core_type.h"

RETRO_BEGIN_DECLS

enum content_mode_load
{
   CONTENT_MODE_LOAD_NONE = 0,
   CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU
};

bool task_push_load_content_with_current_core_from_companion_ui(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_content_from_cli(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_new_core(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_start_builtin_core(content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_start_current_core(content_ctx_info_t *content_info);

bool task_push_start_dummy_core(content_ctx_info_t *content_info);

bool task_push_load_content_with_new_core_from_companion_ui(
      const char *core_path,
      const char *fullpath,
      const char *label,
      content_ctx_info_t *content_info,
      retro_task_callback_t cb,
      void *user_data);

#ifdef HAVE_MENU

bool task_push_load_content_with_new_core_from_menu(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_content_from_playlist_from_menu(
      const char *core_path,
      const char *fullpath,
      const char *label,
      content_ctx_info_t *content_info,
      retro_task_callback_t cb,
      void *user_data);
#endif

bool task_push_load_content_with_core_from_menu(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);
bool task_push_load_subsystem_with_core_from_menu(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

RETRO_END_DECLS

#endif
