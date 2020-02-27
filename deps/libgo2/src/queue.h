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

typedef struct go2_queue go2_queue_t;


#ifdef __cplusplus
extern "C" {
#endif

go2_queue_t* go2_queue_create(int capacity);
int go2_queue_count_get(go2_queue_t* queue);
void go2_queue_push(go2_queue_t* queue, void* value);
void* go2_queue_pop(go2_queue_t* queue);
void go2_queue_destroy(go2_queue_t* queue);

#ifdef __cplusplus
}
#endif
