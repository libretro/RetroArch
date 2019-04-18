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

#if !defined( VCOS_CMD_H )
#define VCOS_CMD_H

/* ---- Include Files ----------------------------------------------------- */

#ifndef VCOS_H
#include "interface/vcos/vcos.h"
#endif
#include "interface/vcos/vcos_stdint.h"

/* ---- Constants and Types ---------------------------------------------- */

struct VCOS_CMD_S;
typedef struct VCOS_CMD_S VCOS_CMD_T;

typedef struct
{
    int         argc;           /* Number of arguments (includes the command/sub-command) */
    char      **argv;           /* Array of arguments */
    char      **argv_orig;      /* Original array of arguments */

    VCOS_CMD_T *cmd_entry;
    VCOS_CMD_T *cmd_parent_entry;

    int         use_log;        /* Output being logged? */
    size_t      result_size;    /* Size of result buffer. */
    char       *result_ptr;     /* Next place to put output. */
    char       *result_buf;     /* Start of the buffer. */

} VCOS_CMD_PARAM_T;

typedef VCOS_STATUS_T (*VCOS_CMD_FUNC_T)( VCOS_CMD_PARAM_T *param );

struct VCOS_CMD_S
{
    const char         *name;
    const char         *args;
    VCOS_CMD_FUNC_T     cmd_fn;
    VCOS_CMD_T         *sub_cmd_entry;
    const char         *descr;

};

/* ---- Variable Externs ------------------------------------------------- */

/* ---- Function Prototypes ---------------------------------------------- */

/*
 * Common printing routine for generating command output.
 */
VCOSPRE_ void VCOSPOST_ vcos_cmd_error( VCOS_CMD_PARAM_T *param, const char *fmt, ... ) VCOS_FORMAT_ATTR_(printf, 2, 3);
VCOSPRE_ void VCOSPOST_ vcos_cmd_printf( VCOS_CMD_PARAM_T *param, const char *fmt, ... ) VCOS_FORMAT_ATTR_(printf, 2, 3);
VCOSPRE_ void VCOSPOST_ vcos_cmd_vprintf( VCOS_CMD_PARAM_T *param, const char *fmt, va_list args ) VCOS_FORMAT_ATTR_(printf, 2, 0);

/*
 * Cause vcos_cmd_error, printf and vprintf to always log to the provided
 * category. When this call is made, the results buffer passed into
 * vcos_cmd_execute is used as a line buffer and does not need to be
 * output by the caller.
 */
struct VCOS_LOG_CAT_T;
VCOSPRE_ void VCOSPOST_ vcos_cmd_always_log_output( struct VCOS_LOG_CAT_T *log_category );

/*
 * Prints command usage for the current command.
 */
VCOSPRE_ void VCOSPOST_ vcos_cmd_usage( VCOS_CMD_PARAM_T *param );

/*
 * Register commands to be processed
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_cmd_register( VCOS_CMD_T *cmd_entry );

/*
 * Registers multiple commands to be processed. The array should
 * be terminated by an entry with all zeros.
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_cmd_register_multiple( VCOS_CMD_T *cmd_entry );

/*
 * Executes a command based on a command line.
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_cmd_execute( int argc, char **argv, size_t result_size, char *result_buf );

/*
 * Shut down the command system and free all allocated data.
 * Do not call any other command functions after this.
 */
VCOSPRE_ void VCOSPOST_ vcos_cmd_shutdown( void );

#endif /* VCOS_CMD_H */
