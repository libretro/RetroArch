/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2019-2020 - James Leaver
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

#ifndef __PLAY_FEATURE_DELIVERY_H
#define __PLAY_FEATURE_DELIVERY_H

#include <retro_common_api.h>
#include <libretro.h>

#include <lists/string_list.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

/* Defines possible status values of
 * a play feature delivery install
 * transaction */
enum play_feature_delivery_install_status
{
   PLAY_FEATURE_DELIVERY_IDLE = 0,
   PLAY_FEATURE_DELIVERY_PENDING,
   PLAY_FEATURE_DELIVERY_STARTING,
   PLAY_FEATURE_DELIVERY_DOWNLOADING,
   PLAY_FEATURE_DELIVERY_INSTALLING,
   PLAY_FEATURE_DELIVERY_INSTALLED,
   PLAY_FEATURE_DELIVERY_FAILED
};

/******************/
/* Initialisation */
/******************/

/* Must be called upon program initialisation */
void play_feature_delivery_init(void);

/* Must be called upon program termination */
void play_feature_delivery_deinit(void);

/**********/
/* Status */
/**********/

/* Returns true if current build utilises
 * play feature delivery */
bool play_feature_delivery_enabled(void);

/* Returns a list of cores currently available
 * via play feature delivery.
 * Returns a new string_list on success, or
 * NULL on failure */
struct string_list *play_feature_delivery_available_cores(void);

/* Returns true if specified core is currently
 * installed via play feature delivery */
bool play_feature_delivery_core_installed(const char *core_file);

/* Fetches last recorded status of the most
 * recently initiated play feature delivery
 * install transaction.
 * 'progress' is an integer from 0-100.
 * Returns true if a transaction is currently
 * in progress. */
bool play_feature_delivery_download_status(
      enum play_feature_delivery_install_status *status,
      unsigned *progress);

/***********/
/* Control */
/***********/

/* Initialises download of the specified core.
 * Returns false in the event of an error.
 * Download status should be monitored via
 * play_feature_delivery_download_status() */
bool play_feature_delivery_download(const char *core_file);

/* Deletes specified core.
 * Returns false in the event of an error. */
bool play_feature_delivery_delete(const char *core_file);

RETRO_END_DECLS

#endif
