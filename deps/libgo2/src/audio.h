#pragma once

/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdint.h>


typedef struct go2_audio go2_audio_t;

typedef enum 
{
    Audio_Path_Off = 0,
    Audio_Path_Speaker,
    Audio_Path_Headphones,
    Audio_Path_SpeakerAndHeadphones,

    Audio_Path_MAX = 0x7fffffff
} go2_audio_path_t;


#ifdef __cplusplus
extern "C" {
#endif

go2_audio_t* go2_audio_create(int frequency);
void go2_audio_destroy(go2_audio_t* audio);
void go2_audio_submit(go2_audio_t* audio, const short* data, int frames);
uint32_t go2_audio_volume_get(go2_audio_t* audio);
void go2_audio_volume_set(go2_audio_t* audio, uint32_t value);
go2_audio_path_t go2_audio_path_get(go2_audio_t* audio);
void go2_audio_path_set(go2_audio_t* audio, go2_audio_path_t value);

#ifdef __cplusplus
}
#endif
