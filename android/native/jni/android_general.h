/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _ANDROID_GENERAL_H
#define _ANDROID_GENERAL_H

#include "android_glue.h"
#include "../../../boolean.h"

struct saved_state
{
    int32_t x;
    int32_t y;
    uint64_t lifecycle_state;
};

struct droid
{
   struct android_app* app;
   unsigned width;
   unsigned height;
   struct saved_state state;
   int32_t last_orient;
   bool window_ready;
   float disp_refresh_rate;
};

struct jni_params
{
   JavaVM *java_vm;
   jobject class_obj;
   char class_name[128];
   char method_name[128];
   char method_signature[128];
   char obj_method_name[128];
   char obj_method_signature[128];
};

struct jni_out_params_char
{
   char *out;
   size_t out_sizeof;
   char in[128];
};

extern struct droid g_android;

void jni_get(void *params, void *out_params, unsigned out_type);
bool android_run_events(struct android_app* android_app);

#endif
