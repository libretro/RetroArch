/*  RetroArch - A frontend for libretro.
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

#ifndef _MMDEVICE_COMMON_H
#define _MMDEVICE_COMMON_H

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * mmdevice_com_init:
 *
 * Brings COM up on the calling thread for the MMDevice API.  The
 * audio drivers run on whichever thread the frontend hands them, and
 * a freshly created worker thread has no apartment: CoCreateInstance
 * on it fails with CO_E_NOTINITIALIZED.
 *
 * Returns true when this call took a reference on the thread's
 * apartment and must be balanced with mmdevice_com_uninit() once
 * every COM object obtained on the thread has been released.
 * Returns false when there is nothing to balance: the thread already
 * has COM in another apartment (usable as it is), or COM could not be
 * brought up at all, which the following CoCreateInstance reports.
 **/
bool mmdevice_com_init(void);

/**
 * mmdevice_com_uninit:
 *
 * Releases the apartment reference taken by mmdevice_com_init() when
 * @init is the value it returned; a no-op for false.
 **/
void mmdevice_com_uninit(bool init);

/**
 * Lists the active endpoints of @data_flow.  Self-contained: brings
 * COM up on the calling thread for its own duration if needed.
 */
void *mmdevice_list_new(const void *u, unsigned data_flow);

/**
 * Gets the friendly name of the provided IMMDevice.
 * The string must be freed with free().
 */
char* mmdevice_name(void *data);

/**
 * Gets the samplerate of the provided IMMDevice.
 */
size_t mmdevice_samplerate(void *data);

/**
 * Gets the handle of the IMMDevice.  The caller must hold COM on the
 * calling thread (see mmdevice_com_init()) for as long as it keeps
 * the returned device.
 */
void *mmdevice_handle(int id, unsigned data_flow);

/**
 * Self-contained like mmdevice_list_new().
 */
size_t mmdevice_get_samplerate(int id);

const char *mmdevice_hresult_name(int hr);

/**
 * Opens endpoint @id (or the default) of @data_flow.  The caller must
 * hold COM on the calling thread (see mmdevice_com_init()) from before
 * this call until the returned device is released.
 */
void *mmdevice_init_device(const char *id, unsigned data_flow);

/**
 * mmdevice_set_active_device:
 *
 * Records @data (an IMMDevice, or NULL to forget) as the endpoint this
 * process holds open for @data_flow (0 render, 1 capture), so the
 * endpoint notification callbacks can tell a state change on a device
 * in use from one on an unrelated device.  Render and capture are
 * tracked separately because both are open at once whenever the
 * microphone driver is running.
 * mmdevice_init_device() calls this itself; drivers that acquire a
 * device another way should call it too.
 **/
void mmdevice_set_active_device(void *data, unsigned data_flow);

#ifdef HAVE_THREADS
void mmdevice_thread(void *data);
#else
DWORD CALLBACK mmdevice_thread(PVOID data);
#endif

RETRO_END_DECLS

#endif
