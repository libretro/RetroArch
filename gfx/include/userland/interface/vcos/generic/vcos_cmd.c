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

/***************************************************************************** 
* 
*    This file provides a generic command line interface which allows
*    vcos internals to be manipulated and/or displayed.
*  
*****************************************************************************/

/* ---- Include Files ---------------------------------------------------- */

#include "interface/vcos/vcos.h"

#ifdef HAVE_VCOS_VERSION
#include "interface/vcos/vcos_build_info.h"
#endif

#ifdef _VIDEOCORE
#include "vcfw/logging/logging.h"
#endif

/* ---- Public Variables ------------------------------------------------- */

/* ---- Private Constants and Types -------------------------------------- */

#define  VCOS_LOG_CATEGORY (&vcos_cmd_log_category)
VCOS_LOG_CAT_T vcos_cmd_log_category;

/* ---- Private Variables ------------------------------------------------ */

static struct VCOS_CMD_GLOBALS_T
{
    VCOS_MUTEX_T    lock;
    VCOS_ONCE_T     initialized;

    unsigned        num_cmd_entries;
    unsigned        num_cmd_alloc;
    VCOS_CMD_T     *cmd_entry;

    VCOS_LOG_CAT_T *log_category;
} cmd_globals;

#ifdef HAVE_VCOS_VERSION
/* Keep the static strings in the image from being dropped by
 * the linker.
 */
extern const char *vcos_get_build_strings(unsigned id);
const char *(*vcos_keep_static_strings)(unsigned);
#endif

/* ---- Private Function Prototypes -------------------------------------- */

static VCOS_STATUS_T help_cmd( VCOS_CMD_PARAM_T *param );

/* ---- Functions  ------------------------------------------------------- */

/***************************************************************************** 
*
*   Walks through the commands looking for a particular command
*
*****************************************************************************/

static VCOS_CMD_T *find_cmd( VCOS_CMD_T *cmd_entry, const char *name )
{
    VCOS_CMD_T   *scan_entry = cmd_entry;

    while ( scan_entry->name != NULL )
    {
        if ( vcos_strcmp( scan_entry->name, name ) == 0 )
        {
            return scan_entry;
        }
        scan_entry++;
    }

    return NULL;
}

/***************************************************************************** 
*
*   Saves away 
*   each line individually.
*
*****************************************************************************/

void vcos_cmd_always_log_output( VCOS_LOG_CAT_T *log_category )
{
    cmd_globals.log_category = log_category;
}

/***************************************************************************** 
*
*   Walks through a buffer containing newline separated lines, and logs
*   each line individually.
*
*****************************************************************************/

static void cmd_log_results( VCOS_CMD_PARAM_T *param )
{
    char    *start;
    char    *end;

    start = end = param->result_buf;

    while ( *start != '\0' )
    {
        while (( *end != '\0' ) && ( *end != '\n' ))
            end++;

        if ( *end == '\n' )
        {
            *end++ = '\0';
        }

        if ( cmd_globals.log_category != NULL )
        {
            if ( vcos_is_log_enabled( cmd_globals.log_category, VCOS_LOG_INFO ))
            {
                vcos_log_impl( cmd_globals.log_category, VCOS_LOG_INFO, "%s", start );
            }
        }
        else
        {
            vcos_log_info( "%s", start );
        }

        start = end;
    }

    /* Since we logged the buffer, reset the pointer back to the beginning. */

    param->result_ptr = param->result_buf;
    param->result_buf[0] = '\0';
}

/***************************************************************************** 
*
*   Since we may have limited output space, we create a generic routine
*   which tries to use the result space, but will switch over to using
*   logging if the output is too large.
*
*****************************************************************************/

void vcos_cmd_vprintf( VCOS_CMD_PARAM_T *param, const char *fmt, va_list args )
{
    int     bytes_written;
    int     bytes_remaining;

    bytes_remaining = (int)(param->result_size - ( param->result_ptr - param->result_buf ));

    bytes_written = vcos_vsnprintf( param->result_ptr, bytes_remaining, fmt, args );

    if ( cmd_globals.log_category != NULL )
    {
        /* We're going to log each line as we encounter it. If the buffer
         * doesn't end in a newline, then we'll wait for one first.
         */

        if ( (( bytes_written + 1 ) >= bytes_remaining ) 
        ||   ( param->result_ptr[ bytes_written - 1 ] == '\n' ))
        {
            cmd_log_results( param );
        }
        else
        {
            param->result_ptr += bytes_written;
        }
    }
    else
    {
        if (( bytes_written + 1 ) >= bytes_remaining )
        {
            /* Output doesn't fit - switch over to logging */

            param->use_log = 1;

            *param->result_ptr = '\0';  /* Zap the partial line that didn't fit above. */

            cmd_log_results( param );   /* resets result_ptr */

            bytes_written = vcos_vsnprintf( param->result_ptr, bytes_remaining, fmt, args );
        }
        param->result_ptr += bytes_written;
    }
}

/***************************************************************************** 
*
*   Prints the output.
*
*****************************************************************************/

void vcos_cmd_printf( VCOS_CMD_PARAM_T *param, const char *fmt, ... )
{
    va_list args;

    va_start( args, fmt );
    vcos_cmd_vprintf( param, fmt, args );
    va_end( args );
}

/***************************************************************************** 
*
*   Prints the arguments which were on the command line prior to ours.
*
*****************************************************************************/

static void print_argument_prefix( VCOS_CMD_PARAM_T *param )
{
    int arg_idx;

    for ( arg_idx = 0; &param->argv_orig[arg_idx] != param->argv; arg_idx++ )
    {
        vcos_cmd_printf( param, "%s ", param->argv_orig[arg_idx] );
    }
}

/***************************************************************************** 
*
*   Prints an error message, prefixed by the command chain required to get
*   to where we're at.
*
*****************************************************************************/

void vcos_cmd_error( VCOS_CMD_PARAM_T *param, const char *fmt, ... )
{
    va_list args;

    print_argument_prefix( param );

    va_start( args, fmt );
    vcos_cmd_vprintf( param, fmt, args );
    va_end( args );
    vcos_cmd_printf( param, "\n" );
}

/****************************************************************************
*
*  usage - prints command usage for an array of commands.
*
***************************************************************************/

static void usage( VCOS_CMD_PARAM_T *param, VCOS_CMD_T *cmd_entry )
{
    int         cmd_idx;
    int         nameWidth = 0;
    int         argsWidth = 0;
    VCOS_CMD_T *scan_entry;

    vcos_cmd_printf( param, "Usage: " );
    print_argument_prefix( param );
    vcos_cmd_printf( param, "command [args ...]\n" );
    vcos_cmd_printf( param, "\n" );
    vcos_cmd_printf( param, "Where command is one of the following:\n" );

    for ( cmd_idx = 0; cmd_entry[cmd_idx].name != NULL; cmd_idx++ )
    {
        int aw;
        int nw;

        scan_entry = &cmd_entry[cmd_idx];

        nw = vcos_strlen( scan_entry->name );
        aw = vcos_strlen( scan_entry->args );

        if ( nw > nameWidth )
        {
            nameWidth = nw;
        }
        if ( aw > argsWidth )
        {
            argsWidth = aw;
        }
    }

    for ( cmd_idx = 0; cmd_entry[cmd_idx].name != NULL; cmd_idx++ )
    {
        scan_entry = &cmd_entry[cmd_idx];

        vcos_cmd_printf( param, "  %-*s %-*s - %s\n", 
                    nameWidth, scan_entry->name,
                    argsWidth, scan_entry->args,
                    scan_entry->descr );
    }
}

/****************************************************************************
*
*  Prints the usage for the current command.
*
***************************************************************************/

void vcos_cmd_usage( VCOS_CMD_PARAM_T *param )
{
    VCOS_CMD_T *cmd_entry;

    cmd_entry = param->cmd_entry;

    if ( cmd_entry->sub_cmd_entry != NULL )
    {
        /* This command is command with sub-commands */

        usage( param, param->cmd_entry->sub_cmd_entry );
    }
    else
    {
        vcos_cmd_printf( param, "Usage: " );
        print_argument_prefix( param );
        vcos_cmd_printf( param, "%s %s - %s\n",
                         param->argv[0],
                         param->cmd_entry->args,
                         param->cmd_entry->descr );
    }
}

/***************************************************************************** 
*
*   Command to print out the help
* 
*   This help command is only called from the main menu.
* 
*****************************************************************************/

static VCOS_STATUS_T help_cmd( VCOS_CMD_PARAM_T *param )
{
    VCOS_CMD_T  *found_entry;

#if 0
    {
        int arg_idx;

        vcos_log_trace( "%s: argc = %d", __func__, param->argc );
        for ( arg_idx = 0; arg_idx < param->argc; arg_idx++ )
        {
            vcos_log_trace( "%s:  argv[%d] = '%s'", __func__, arg_idx, param->argv[arg_idx] );
        }
    }
#endif

    /* If there is an argument after the word help, then we want to print
     * help for that command.
     */

    if ( param->argc == 1 )
    {
        if ( param->cmd_parent_entry == cmd_globals.cmd_entry )
        {
            /* Bare help - print the command usage for the root */

            usage( param, cmd_globals.cmd_entry );
            return VCOS_SUCCESS;
        }

        /* For all other cases help requires an argument */
            
        vcos_cmd_error( param, "%s requires an argument", param->argv[0] );
        return VCOS_EINVAL;
    }

    /* We were given an argument. */

    if (( found_entry = find_cmd( param->cmd_parent_entry, param->argv[1] )) != NULL )
    {
        /* Make it look like the command that was specified is the one that's
         * currently running
         */

        param->cmd_entry = found_entry;
        param->argv[0] = param->argv[1];
        param->argv++;
        param->argc--;

        vcos_cmd_usage( param );
        return VCOS_SUCCESS;
    }

    vcos_cmd_error( param, "- unrecognized command: '%s'", param->argv[1] );
    return VCOS_ENOENT;
}

/***************************************************************************** 
*
*   Command to print out the version/build information.
*
*****************************************************************************/

#ifdef HAVE_VCOS_VERSION

static VCOS_STATUS_T version_cmd( VCOS_CMD_PARAM_T *param )
{
    static const char* copyright = "Copyright (c) 2011 Broadcom";

    vcos_cmd_printf( param, "%s %s\n%s\nversion %s\nhost %s",
                     vcos_get_build_date(),
                     vcos_get_build_time(),
                     copyright,
                     vcos_get_build_version(),
                     vcos_get_build_hostname() );

    return VCOS_SUCCESS;
}

#endif

/*****************************************************************************
*
*   Internal commands
*
*****************************************************************************/

static VCOS_CMD_T cmd_help    = { "help",    "[command]", help_cmd,    NULL, "Prints command help information" };

#ifdef HAVE_VCOS_VERSION
static VCOS_CMD_T cmd_version = { "version", "",          version_cmd, NULL, "Prints build/version information" };
#endif

/***************************************************************************** 
*
*   Walks the command table and executes the commands
*
*****************************************************************************/

static VCOS_STATUS_T execute_cmd( VCOS_CMD_PARAM_T *param, VCOS_CMD_T *cmd_entry )
{
    const char     *cmdStr;
    VCOS_CMD_T     *found_entry;

#if 0
    {
        int arg_idx;

        vcos_cmd_printf( param, "%s: argc = %d", __func__, param->argc );
        for ( arg_idx = 0; arg_idx < param->argc; arg_idx++ )
        {
            vcos_cmd_printf( param, " argv[%d] = '%s'", arg_idx, param->argv[arg_idx] );
        }
        vcos_cmd_printf( param, "\n" );
    }
#endif

    if ( param->argc <= 1 )
    {
        /* No command specified */

        vcos_cmd_error( param, "%s - no command specified", param->argv[0] );
        return VCOS_EINVAL;
    }

    /* argv[0] is the command/program that caused us to get invoked, so we strip
     * it off.
     */

    param->argc--;
    param->argv++;
    param->cmd_parent_entry = cmd_entry;

    /* Not the help command, scan for the command and execute it. */

    cmdStr = param->argv[0];

    if (( found_entry = find_cmd( cmd_entry, cmdStr )) != NULL )
    {
        if ( found_entry->sub_cmd_entry != NULL )
        {
            return execute_cmd( param, found_entry->sub_cmd_entry );
        }

        param->cmd_entry = found_entry;
        return found_entry->cmd_fn( param );
    }

    /* Unrecognized command - check to see if it was the help command */

    if ( vcos_strcmp( cmdStr, cmd_help.name ) == 0 )
    {
        return help_cmd( param );
    }

    vcos_cmd_error( param, "- unrecognized command: '%s'", cmdStr );
    return VCOS_ENOENT;
}

/***************************************************************************** 
*
*   Initializes the command line parser.
*
*****************************************************************************/

static void vcos_cmd_init( void )
{
    vcos_mutex_create( &cmd_globals.lock, "vcos_cmd" );

    cmd_globals.num_cmd_entries = 0;
    cmd_globals.num_cmd_alloc = 0;
    cmd_globals.cmd_entry = NULL;

#ifdef HAVE_VCOS_VERSION
   vcos_keep_static_strings = vcos_get_build_strings;
#endif
}

/***************************************************************************** 
*
*   Shuts down the command line parser.
*
*****************************************************************************/

void vcos_cmd_shutdown( void )
{
    vcos_mutex_delete( &cmd_globals.lock );

    vcos_free( cmd_globals.cmd_entry );
    cmd_globals.cmd_entry = NULL;
}

/*****************************************************************************
*
*   Command line processor.
*
*****************************************************************************/

VCOS_STATUS_T vcos_cmd_execute( int argc, char **argv, size_t result_size, char *result_buf )
{
    VCOS_STATUS_T       rc = VCOS_EINVAL;
    VCOS_CMD_PARAM_T    param;

    vcos_once( &cmd_globals.initialized, vcos_cmd_init );

    param.argc = argc;
    param.argv = param.argv_orig = argv;

    param.use_log = 0;
    param.result_size = result_size;
    param.result_ptr = result_buf;
    param.result_buf = result_buf;

	result_buf[0] = '\0';

    vcos_mutex_lock( &cmd_globals.lock );

    rc = execute_cmd( &param, cmd_globals.cmd_entry );

    if ( param.use_log )
    {
        cmd_log_results( &param );
        vcos_snprintf( result_buf, result_size, "results logged" );
    }
    else
    if ( cmd_globals.log_category != NULL )
    {
        if ( result_buf[0] != '\0' )
        {
            /* There is a partial line still buffered. */

            vcos_cmd_printf( &param, "\n" );
        }
    }

    vcos_mutex_unlock( &cmd_globals.lock );

    return rc;
}

/***************************************************************************** 
*
*   Registers a command entry with the command line processor
*
*****************************************************************************/

VCOS_STATUS_T vcos_cmd_register( VCOS_CMD_T *cmd_entry )
{
    VCOS_STATUS_T   rc;
    VCOS_UNSIGNED   new_num_cmd_alloc;
    VCOS_CMD_T     *new_cmd_entry;
    VCOS_CMD_T     *old_cmd_entry;
    VCOS_CMD_T     *scan_entry;

    vcos_once( &cmd_globals.initialized, vcos_cmd_init );

    vcos_assert( cmd_entry != NULL );
    vcos_assert( cmd_entry->name != NULL );

    vcos_log_trace( "%s: cmd '%s'", __FUNCTION__, cmd_entry->name );

    vcos_assert( cmd_entry->args != NULL );
    vcos_assert(( cmd_entry->cmd_fn != NULL ) || ( cmd_entry->sub_cmd_entry != NULL ));
    vcos_assert( cmd_entry->descr != NULL );

    /* We expect vcos_cmd_init to be called before vcos_logging_init, so we
     * need to defer registering our logging category until someplace
     * like right here.
     */

    if ( vcos_cmd_log_category.name == NULL )
    {
        /*
         * If you're using the command interface, you pretty much always want
         * log messages from this file to show up. So we change the default
         * from ERROR to be the more reasonable INFO level.
         */

        vcos_log_set_level(&vcos_cmd_log_category, VCOS_LOG_INFO);
        vcos_log_register("vcos_cmd", &vcos_cmd_log_category);

        /* We register a help command so that it shows up in the usage. */

        vcos_cmd_register( &cmd_help );
#ifdef HAVE_VCOS_VERSION
        vcos_cmd_register( &cmd_version );
#endif
    }

    vcos_mutex_lock( &cmd_globals.lock );

    if ( cmd_globals.num_cmd_entries >= cmd_globals.num_cmd_alloc )
    {
        if ( cmd_globals.num_cmd_alloc == 0 )
        {
            /* We haven't allocated a table yet */
        }

        /* The number 8 is rather arbitrary. */

        new_num_cmd_alloc = cmd_globals.num_cmd_alloc + 8;

        /* The + 1 is to ensure that we always have a NULL entry at the end. */

        new_cmd_entry = (VCOS_CMD_T *)vcos_calloc( new_num_cmd_alloc + 1, sizeof( *cmd_entry ), "vcos_cmd_entries" );
        if ( new_cmd_entry == NULL )
        {
            rc = VCOS_ENOMEM;
            goto out;
        }
        memcpy( new_cmd_entry, cmd_globals.cmd_entry, cmd_globals.num_cmd_entries * sizeof( *cmd_entry ));
        cmd_globals.num_cmd_alloc = new_num_cmd_alloc;
        old_cmd_entry = cmd_globals.cmd_entry;
        cmd_globals.cmd_entry = new_cmd_entry;
        vcos_free( old_cmd_entry );
    }

    if ( cmd_globals.num_cmd_entries == 0 )
    {
        /* This is the first command being registered */

        cmd_globals.cmd_entry[0] = *cmd_entry;
    }
    else
    {
        /* Keep the list in alphabetical order. We start at the end and work backwards
         * shuffling entries up one until we find an insertion point.
         */

        for ( scan_entry = &cmd_globals.cmd_entry[cmd_globals.num_cmd_entries - 1];
              scan_entry >= cmd_globals.cmd_entry; scan_entry-- )
        {
            if ( vcos_strcmp( cmd_entry->name, scan_entry->name ) > 0 )
            {
                /* We found an insertion point. */

                break;
            }

            scan_entry[1] = scan_entry[0];
        }
        scan_entry[1] = *cmd_entry;
    }
    cmd_globals.num_cmd_entries++;

    rc = VCOS_SUCCESS;

out:

    vcos_mutex_unlock( &cmd_globals.lock );
    return rc;
}

/***************************************************************************** 
*
*   Registers multiple commands.
*
*****************************************************************************/

VCOS_STATUS_T vcos_cmd_register_multiple( VCOS_CMD_T *cmd_entry )
{
    VCOS_STATUS_T   status;

    while ( cmd_entry->name != NULL )
    {
        if (( status = vcos_cmd_register( cmd_entry )) != VCOS_SUCCESS )
        {
            return status;
        }
        cmd_entry++;
    }
    return VCOS_SUCCESS;
}

