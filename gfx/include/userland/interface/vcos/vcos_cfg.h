/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#if !defined( VCOS_CFG_H )
#define VCOS_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

typedef struct opaque_vcos_cfg_buf_t    *VCOS_CFG_BUF_T;
typedef struct opaque_vcos_cfg_entry_t  *VCOS_CFG_ENTRY_T;

/** \file vcos_file.h
  *
  * API for accessing configuration/statistics information. This
  * is loosely modelled on the linux proc entries.
  */

typedef void (*VCOS_CFG_SHOW_FPTR)( VCOS_CFG_BUF_T buf, void *data );
typedef void (*VCOS_CFG_PARSE_FPTR)( VCOS_CFG_BUF_T buf, void *data );

/** Create a configuration directory.
  *
  * @param entry        Place to store the created config entry.
  * @param parent       Parent entry (for directory like config
  *                     options).
  * @param entryName    Name of the directory.
  */

VCOS_STATUS_T vcos_cfg_mkdir( VCOS_CFG_ENTRY_T *entry,
                              VCOS_CFG_ENTRY_T *parent,
                              const char *dirName );

/** Create a configuration entry.
  *
  * @param entry        Place to store the created config entry.
  * @param parent       Parent entry (for directory like config
  *                     options).
  * @param entryName    Name of the configuration entry.
  * @param showFunc     Function pointer to show configuration
  *                     data.
  * @param parseFunc    Function pointer to parse new data.
  */

VCOS_STATUS_T vcos_cfg_create_entry( VCOS_CFG_ENTRY_T *entry,
                                     VCOS_CFG_ENTRY_T *parent,
                                     const char *entryName,
                                     VCOS_CFG_SHOW_FPTR showFunc,
                                     VCOS_CFG_PARSE_FPTR parseFunc,
                                     void *data );

/** Determines if a configuration entry has been created or not.
  *
  * @param entry        Configuration entry to query.
  */

int vcos_cfg_is_entry_created( VCOS_CFG_ENTRY_T entry );

/** Returns the name of a configuration entry.
  *
  * @param entry        Configuration entry to query.
  */

const char *vcos_cfg_get_entry_name( VCOS_CFG_ENTRY_T entry );

/** Removes a configuration entry.
  *
  * @param entry        Configuration entry to remove.
  */

VCOS_STATUS_T vcos_cfg_remove_entry( VCOS_CFG_ENTRY_T *entry );

/** Writes data into a configuration buffer. Only valid inside
  * the show function.
  *
  * @param buf      Buffer to write data into.
  * @param fmt      printf style format string.
  */

void vcos_cfg_buf_printf( VCOS_CFG_BUF_T buf, const char *fmt, ... );

/** Retrieves a null terminated string of the data associated
  * with the buffer. Only valid inside the parse function.
  *
  * @param buf      Buffer to get data from.
  * @param fmt      printf style format string.
  */

char *vcos_cfg_buf_get_str( VCOS_CFG_BUF_T buf );

void *vcos_cfg_get_proc_entry( VCOS_CFG_ENTRY_T entry );

#ifdef __cplusplus
}
#endif
#endif
