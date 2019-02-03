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

/*=============================================================================
Categorized logging for VCOS - a generic implementation.
=============================================================================*/

#include "interface/vcos/vcos.h"
#include "interface/vcos/vcos_ctype.h"
#include "interface/vcos/vcos_string.h"
#include "interface/vcos/vcos_inttypes.h"

static VCOS_MUTEX_T lock;
static int warned_loglevel;             /* only warn about invalid log level once */
static VCOS_VLOG_IMPL_FUNC_T vcos_vlog_impl_func = vcos_vlog_default_impl;

#define  VCOS_LOG_CATEGORY (&dflt_log_category)
static VCOS_LOG_CAT_T dflt_log_category;
VCOS_LOG_CAT_T *vcos_logging_categories = NULL;
static int inited;

#if VCOS_HAVE_CMD

/*
 * For kernel or videocore purposes, we generally want the log command. For
 * user-space apps, they might want to provide their own log command, so we
 * don't include the built in on.
 *
 * So pthreads/vcos_platform.h defines VCOS_WANT_LOG_CMD to be 0. It is
 * undefined elsewhere.
 */

#  if !defined( VCOS_WANT_LOG_CMD )
#     define  VCOS_WANT_LOG_CMD 1
#  endif
#else
#  define VCOS_WANT_LOG_CMD   0
#endif

#if VCOS_WANT_LOG_CMD

/*****************************************************************************
*
*   Does a vcos_assert(0), which is useful to test logging.
*
*****************************************************************************/

VCOS_STATUS_T vcos_log_assert_cmd( VCOS_CMD_PARAM_T *param )
{
   (void)param;

#if defined( NDEBUG ) && !defined( VCOS_RELEASE_ASSERTS )
   vcos_log_error( "vcos_asserts have been compiled out" );
   vcos_cmd_printf( param, "vcos_asserts have been compiled out - did a vcos_log_error instead\n" );
#else
   vcos_assert(0);
   vcos_cmd_printf( param, "Executed vcos_assert(0)\n" );
#endif

   return VCOS_SUCCESS;
}

/*****************************************************************************
*
*   Sets a vcos logging level
*
*****************************************************************************/

VCOS_STATUS_T vcos_log_set_cmd( VCOS_CMD_PARAM_T *param )
{
   VCOS_LOG_CAT_T   *cat;
   char             *name;
   char             *levelStr;
   VCOS_LOG_LEVEL_T  level;
   VCOS_STATUS_T     status;

   if ( param->argc != 3 )
   {
      vcos_cmd_usage( param );
      return VCOS_EINVAL;
   }

   name = param->argv[1];
   levelStr = param->argv[2];

   if ( vcos_string_to_log_level( levelStr, &level ) != VCOS_SUCCESS )
   {
      vcos_cmd_printf( param, "Unrecognized logging level: '%s'\n", levelStr );
      return VCOS_EINVAL;
   }

   vcos_mutex_lock(&lock);

   status = VCOS_SUCCESS;
   for ( cat = vcos_logging_categories; cat != NULL; cat = cat->next )
   {
      if ( vcos_strcmp( name, cat->name ) == 0 )
      {
         cat->level = level;
         vcos_cmd_printf( param, "Category %s level set to %s\n", name, levelStr );
         break;
      }
      else if ( vcos_strcmp( name, "*") == 0 )
      {
         cat->level = level;
         vcos_cmd_printf( param, "Category %s level set to %s\n", name, levelStr );
      }
   }
   if ( cat == NULL )
   {
      vcos_cmd_printf( param, "Unrecognized category: '%s'\n", name );
      status = VCOS_ENOENT;
   }

   vcos_mutex_unlock(&lock);

   return status;
}

/*****************************************************************************
*
*   Prints out the current settings for a given category (or all cvategories)
*
*****************************************************************************/

VCOS_STATUS_T vcos_log_status_cmd( VCOS_CMD_PARAM_T *param )
{
   VCOS_LOG_CAT_T   *cat;
   VCOS_STATUS_T     status;

   vcos_mutex_lock(&lock);

   if ( param->argc == 1)
   {
      int   nw;
      int   nameWidth = 0;

      /* Print information about all of the categories. */

      for ( cat = vcos_logging_categories; cat != NULL; cat = cat->next )
      {
         nw = (int)strlen( cat->name );

         if ( nw > nameWidth )
         {
            nameWidth = nw;
         }
      }

      for ( cat = vcos_logging_categories; cat != NULL; cat = cat->next )
      {
         vcos_cmd_printf( param, "%-*s - %s\n", nameWidth, cat->name, vcos_log_level_to_string( cat->level ));
      }
   }
   else
   {
      /* Print information about a particular category */

      for ( cat = vcos_logging_categories; cat != NULL; cat = cat->next )
      {
         if ( vcos_strcmp( cat->name, param->argv[1] ) == 0 )
         {
            vcos_cmd_printf( param, "%s - %s\n", cat->name, vcos_log_level_to_string( cat->level ));
            break;
         }
      }
      if ( cat == NULL )
      {
         vcos_cmd_printf( param, "Unrecognized logging category: '%s'\n", param->argv[1] );
         status = VCOS_ENOENT;
         goto out;
      }
   }

   status = VCOS_SUCCESS;
out:
   vcos_mutex_unlock(&lock);

   return status;
}

/*****************************************************************************
*
*   Prints out the current settings for a given category (or all cvategories)
*
*****************************************************************************/

VCOS_STATUS_T vcos_log_test_cmd( VCOS_CMD_PARAM_T *param )
{
   if ( param->argc == 1 )
   {
      static   int seq_num = 100;

      /* No additional arguments - generate a message with an incrementing number */

      vcos_log_error( "Test message %d", seq_num );

      seq_num++;
      vcos_cmd_printf( param, "Logged 'Test message %d'\n", seq_num );
   }
   else
   {
      int   arg_idx;

      /* Arguments supplied - log these */

      for ( arg_idx = 0; arg_idx < param->argc; arg_idx++ )
      {
         vcos_log_error( "argv[%d] = '%s'", arg_idx, param->argv[arg_idx] );
      }
      vcos_cmd_printf( param, "Logged %d line(s) of test data\n", param->argc );
   }
   return VCOS_SUCCESS;
}

/*****************************************************************************
*
*   Internal commands
*
*****************************************************************************/

static VCOS_CMD_T log_cmd_entry[] =
{
    { "assert",   "",                  vcos_log_assert_cmd, NULL,    "Does a vcos_assert(0) to test logging" },
    { "set",      "category level",    vcos_log_set_cmd,    NULL,    "Sets the vcos logging level for a category" },
    { "status",   "[category]",        vcos_log_status_cmd, NULL,    "Prints the vcos log status for a (or all) categories" },
    { "test",     "[arbitrary text]",  vcos_log_test_cmd,   NULL,    "Does a vcos_log to test logging" },

    { NULL,       NULL,                NULL,                NULL,    NULL }
};

static VCOS_CMD_T cmd_log =
    { "log",        "command [args]",  NULL,    log_cmd_entry, "Commands related to vcos logging" };

#endif

void vcos_logging_init(void)
{
   if (inited)
   {
      /* FIXME: should print a warning or something here */
      return;
   }
   vcos_mutex_create(&lock, "vcos_log");

   vcos_log_platform_init();

   vcos_log_register("default", &dflt_log_category);

#if VCOS_WANT_LOG_CMD
   vcos_cmd_register( &cmd_log );
#endif

   vcos_assert(!inited);
   inited = 1;
}

/** Read an alphanumeric token, returning True if we succeeded.
  */

static int read_tok(char *tok, size_t toklen, const char **pstr, char sep)
{
   const char *str = *pstr;
   size_t n = 0;
   char ch;

   /* skip past any whitespace */
   while (str[0] && isspace((int)(str[0])))
      str++;

   while ((ch = *str) != '\0' &&
          ch != sep &&
          (isalnum((int)ch) || (ch == '_') || (ch == '*')) &&
          n != toklen-1)
   {
      tok[n++] = ch;
      str++;
   }

   /* did it work out? */
   if (ch == '\0' || ch == sep)
   {
      if (ch) str++; /* move to next token if not at end */
      /* yes */
      tok[n] = '\0';
      *pstr = str;
      return 1;
   }
   else
   {
      /* no */
      return 0;
   }
}

const char *vcos_log_level_to_string( VCOS_LOG_LEVEL_T level )
{
   switch (level)
   {
      case VCOS_LOG_UNINITIALIZED:  return "uninit";
      case VCOS_LOG_NEVER:          return "never";
      case VCOS_LOG_ERROR:          return "error";
      case VCOS_LOG_WARN:           return "warn";
      case VCOS_LOG_INFO:           return "info";
      case VCOS_LOG_TRACE:          return "trace";
   }
   return "???";
}

VCOS_STATUS_T vcos_string_to_log_level( const char *str, VCOS_LOG_LEVEL_T *level )
{
   if (strcmp(str,"error") == 0)
      *level = VCOS_LOG_ERROR;
   else if (strcmp(str,"never") == 0)
      *level = VCOS_LOG_NEVER;
   else if (strcmp(str,"warn") == 0)
      *level = VCOS_LOG_WARN;
   else if (strcmp(str,"warning") == 0)
      *level = VCOS_LOG_WARN;
   else if (strcmp(str,"info") == 0)
      *level = VCOS_LOG_INFO;
   else if (strcmp(str,"trace") == 0)
      *level = VCOS_LOG_TRACE;
   else
      return VCOS_EINVAL;

   return VCOS_SUCCESS;
}

static int read_level(VCOS_LOG_LEVEL_T *level, const char **pstr, char sep)
{
   char buf[16];
   int ret = 1;
   if (read_tok(buf,sizeof(buf),pstr,sep))
   {
      if (vcos_string_to_log_level(buf,level) != VCOS_SUCCESS)
      {
         vcos_log("Invalid trace level '%s'\n", buf);
         ret = 0;
      }
   }
   else
   {
      ret = 0;
   }
   return ret;
}

void vcos_log_register(const char *name, VCOS_LOG_CAT_T *category)
{
   const char *env;
   VCOS_LOG_CAT_T *i;

   category->name  = name;
   if ( category->level == VCOS_LOG_UNINITIALIZED )
   {
      category->level = VCOS_LOG_ERROR;
   }
   category->flags.want_prefix = (category != &dflt_log_category );

   vcos_mutex_lock(&lock);

   /* is it already registered? */
   for (i = vcos_logging_categories; i ; i = i->next )
   {
      if (i == category)
      {
         i->refcount++;
         break;
      }
   }

   if (!i)
   {
      /* not yet registered */
      category->next = vcos_logging_categories;
      vcos_logging_categories = category;
      category->refcount++;

      vcos_log_platform_register(category);
   }

   vcos_mutex_unlock(&lock);

   /* Check to see if this log level has been enabled. Look for
    * (<category:level>,)*
    *
    * VC_LOGLEVEL=ilcs:info,vchiq:warn
    */

   env = _VCOS_LOG_LEVEL();
   if (env && env[0])
   {
      do
      {
         char env_name[64];
         VCOS_LOG_LEVEL_T level;
         if (read_tok(env_name, sizeof(env_name), &env, ':') &&
             read_level(&level, &env, ','))
         {
            if (strcmp(env_name, name) == 0 || strcmp(env_name, "*") == 0)
            {
               // we could match both * and env_name, so make sure * comes
               // first in the logging_level string
               category->level = level;
            }
         }
         else
         {
            if (!warned_loglevel)
            {
                vcos_log("VC_LOGLEVEL format invalid at %s\n", env);
                warned_loglevel = 1;
            }
            return;
         }
      } while (env[0] != '\0');
   }

   vcos_log_info( "Registered log category '%s' with level %s",
                  category->name,
                  vcos_log_level_to_string( category->level ));
}

void vcos_log_unregister(VCOS_LOG_CAT_T *category)
{
   VCOS_LOG_CAT_T **pcat;

   vcos_mutex_lock(&lock);
   category->refcount--;
   if (category->refcount == 0)
   {
      pcat = &vcos_logging_categories;
      while (*pcat != category)
      {
         if (!*pcat)
            break;   /* possibly deregistered twice? */
         if ((*pcat)->next == NULL)
         {
            vcos_assert(0); /* already removed! */
            vcos_mutex_unlock(&lock);
            return;
         }
         pcat = &(*pcat)->next;
      }
      if (*pcat)
         *pcat = category->next;

      vcos_log_platform_unregister(category);
   }
   vcos_mutex_unlock(&lock);
}

VCOSPRE_ const VCOS_LOG_CAT_T * VCOSPOST_ vcos_log_get_default_category(void)
{
   return &dflt_log_category;
}

void vcos_set_log_options(const char *opt)
{
   (void)opt;
}

void vcos_log_dump_mem_impl( const VCOS_LOG_CAT_T *cat,
                             const char           *label,
                             uint32_t              addr,
                             const void           *voidMem,
                             size_t                numBytes )
{
   const uint8_t  *mem = (const uint8_t *)voidMem;
   size_t          offset;
   char            lineBuf[ 100 ];
   char           *s;

   while ( numBytes > 0 )
   {
       s = lineBuf;

       for ( offset = 0; offset < 16; offset++ )
       {
           if ( offset < numBytes )
           {
               s += vcos_snprintf( s, 4, "%02x ", mem[ offset ]);
           }
           else
           {
               s += vcos_snprintf( s, 4, "   " );
           }
       }

       for ( offset = 0; offset < 16; offset++ )
       {
           if ( offset < numBytes )
           {
               uint8_t ch = mem[ offset ];

               if (( ch < ' ' ) || ( ch > '~' ))
               {
                   ch = '.';
               }
               *s++ = (char)ch;
           }
       }
       *s++ = '\0';

       if (( label != NULL ) && ( *label != '\0' ))
       {
          vcos_log_impl( cat, VCOS_LOG_INFO, "%s: %08" PRIx32 ": %s", label, addr, lineBuf );
       }
       else
       {
          vcos_log_impl( cat, VCOS_LOG_INFO, "%08" PRIx32 ": %s", addr, lineBuf );
       }

       addr += 16;
       mem += 16;
       if ( numBytes > 16 )
       {
           numBytes -= 16;
       }
       else
       {
           numBytes = 0;
       }
   }

}

void vcos_log_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, ...)
{
   va_list ap;
   va_start(ap,fmt);
   vcos_vlog_impl( cat, _level, fmt, ap );
   va_end(ap);
}

void vcos_vlog_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, va_list args)
{
   vcos_vlog_impl_func( cat, _level, fmt, args );
}

void vcos_set_vlog_impl( VCOS_VLOG_IMPL_FUNC_T vlog_impl_func )
{
   if ( vlog_impl_func == NULL )
   {
      vcos_vlog_impl_func = vcos_vlog_default_impl;
   }
   else
   {
      vcos_vlog_impl_func = vlog_impl_func;
   }
}
