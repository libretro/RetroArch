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
VideoCore OS Abstraction Layer - logging support
=============================================================================*/

#ifndef VCOS_LOGGING_H
#define VCOS_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#include "interface/vcos/vcos_types.h"
#include "vcos.h"
#include "vcos_logging_control.h"

/**
 * \file
 *
 * Logging support
 *
 * This provides categorised logging. Clients register
 * a category, and then get a number of logging levels for
 * that category.
 *
 * The logging level flag is tested using a flag *before* the
 * function call, which makes logging very fast when disabled - there
 * is no function call overhead just to find out that this log
 * message is disabled.
 *
 * \section VCOS_LOG_CATEGORY
 *
 * As a convenience, clients define VCOS_LOG_CATEGORY to point to
 * their category; the various vcos_log_xxx() macros then expand to
 * use this.
 *
 * e.g.
 *
 *     #define VCOS_LOG_CATEGORY (&my_category)
 *
 *     #include <interface/vcos/vcos.h>
 *
 *     VCOS_LOG_CAT_T my_category;
 *
 *     ....
 *
 *     vcos_log_trace("Stuff happened: %d", n_stuff);
 *
 */

/** Logging levels */
typedef enum VCOS_LOG_LEVEL_T
{
   VCOS_LOG_UNINITIALIZED   = 0,
   VCOS_LOG_NEVER,
   VCOS_LOG_ERROR,
   VCOS_LOG_WARN,
   VCOS_LOG_INFO,
   VCOS_LOG_TRACE,
} VCOS_LOG_LEVEL_T;


/** Initialize a logging category without going through vcos_log_register().
 *
 * This is useful for the case where there is no obvious point to do the
 * registration (no initialization function for the module). However, it
 * means that your logging category is not registered, so cannot be easily
 * changed at run-time.
 */
#define VCOS_LOG_INIT(n,l) { l, n, 0, {0}, 0, 0 }

/** A registered logging category.
  */
typedef struct VCOS_LOG_CAT_T
{
   VCOS_LOG_LEVEL_T level;      /**< Which levels are enabled for this category */
   const char *name;            /**< Name for this category. */
   struct VCOS_LOG_CAT_T *next;
   struct {
      unsigned int want_prefix:1;
   } flags;
   unsigned int refcount;
   void *platform_data;         /**< platform specific data */
} VCOS_LOG_CAT_T;

typedef void (*VCOS_VLOG_IMPL_FUNC_T)(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, va_list args);

/** Convert a VCOS_LOG_LEVEL_T into a printable string.
  * The platform needs to implement this function.
  */
VCOSPRE_ const char * VCOSPOST_ vcos_log_level_to_string( VCOS_LOG_LEVEL_T level );

/** Convert a string into a VCOS_LOG_LEVEL_T
  * The platform needs to implement this function.
  */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_string_to_log_level( const char *str, VCOS_LOG_LEVEL_T *level );

/** Log a message. Basic API. Normal code should not use this.
  * The platform needs to implement this function.
  */
VCOSPRE_ void VCOSPOST_ vcos_log_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, ...) VCOS_FORMAT_ATTR_(printf, 3, 4);

/** Log a message using a varargs parameter list. Normal code should
  * not use this.
  */
VCOSPRE_ void VCOSPOST_ vcos_vlog_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, va_list args) VCOS_FORMAT_ATTR_(printf, 3, 0);

/** Set the function which does the actual logging output.
 *  Passing in NULL causes the default logging function to be
 *  used.
  */
VCOSPRE_ void VCOSPOST_ vcos_set_vlog_impl( VCOS_VLOG_IMPL_FUNC_T vlog_impl_func );

/** The default logging function, which is provided by each
  * platform.
  */

VCOSPRE_ void VCOSPOST_ vcos_vlog_default_impl(const VCOS_LOG_CAT_T *cat, VCOS_LOG_LEVEL_T _level, const char *fmt, va_list args) VCOS_FORMAT_ATTR_(printf, 3, 0);

/*
 * Initialise the logging subsystem. This is called from
 * vcos_init() so you don't normally need to call it.
 */
VCOSPRE_ void VCOSPOST_ vcos_logging_init(void);

/** Register a logging category.
  *
  * @param name the name of this category.
  * @param category the category to register.
  */
VCOSPRE_ void VCOSPOST_ vcos_log_register(const char *name, VCOS_LOG_CAT_T *category);

/** Unregister a logging category.
  */
VCOSPRE_ void VCOSPOST_ vcos_log_unregister(VCOS_LOG_CAT_T *category);

/** Return a default logging category, for people too lazy to create their own.
  *
  * Using the default category will be slow (there's an extra function
  * call overhead). Don't do this in normal code.
  */
VCOSPRE_ const VCOS_LOG_CAT_T * VCOSPOST_ vcos_log_get_default_category(void);

VCOSPRE_ void VCOSPOST_ vcos_set_log_options(const char *opt);

/** Set the logging level for a category at run time. Without this, the level
  * will be that set by vcos_log_register from a platform-specific source.
  *
  * @param category the category to modify.
  * @param level the new logging level for this category.
  */
VCOS_STATIC_INLINE void vcos_log_set_level(VCOS_LOG_CAT_T *category, VCOS_LOG_LEVEL_T level)
{
   category->level = level;
}


/** Set the logging levels for many categories at once.
  *
  * Set lots of categorys from a string cat:lev,cat:lev This can 
  * be used at startup time to set a bunch of category levels from
  * a single string.  Used with C(vcos_logging_level)
  */
VCOSPRE_ void VCOSPOST_ vcos_log_set_level_all(char *levels);

#define vcos_log_dump_mem(cat,label,addr,voidMem,numBytes)  do { if (vcos_is_log_enabled(cat,VCOS_LOG_TRACE)) vcos_log_dump_mem_impl(cat,label,addr,voidMem,numBytes); } while (0)

void vcos_log_dump_mem_impl( const VCOS_LOG_CAT_T *cat,
                             const char           *label,
                             uint32_t              addr,
                             const void           *voidMem,
                             size_t                numBytes );

/*
 * Platform specific hooks (optional).
 */
#ifndef vcos_log_platform_init
#define vcos_log_platform_init()                (void)0
#endif

#ifndef vcos_log_platform_register
#define vcos_log_platform_register(category)    (void)0
#endif

#ifndef vcos_log_platform_unregister
#define vcos_log_platform_unregister(category)  (void)0
#endif

/* VCOS_TRACE() - deprecated macro which just outputs in a debug build and
 * is a no-op in a release build.
 *
 * _VCOS_LOG_X() - internal macro which outputs if the current level for the
 * particular category is higher than the supplied message level.
 */

#define VCOS_LOG_DFLT_CATEGORY vcos_log_get_default_category()

#define _VCOS_LEVEL(x) (x)

#define vcos_is_log_enabled(cat,_level)  (_VCOS_LEVEL((cat)->level) >= _VCOS_LEVEL(_level))

#if defined(_VCOS_METAWARE) || defined(__GNUC__)

# if !defined(AMPUTATE_ALL_VCOS_LOGGING) && (!defined(NDEBUG) || defined(VCOS_ALWAYS_WANT_LOGGING))
#  define VCOS_LOGGING_ENABLED
#  define _VCOS_LOG_X(cat, _level, fmt...)   do { if (vcos_is_log_enabled(cat,_level)) vcos_log_impl(cat,_level,fmt); } while (0)
#  define _VCOS_VLOG_X(cat, _level, fmt, ap) do { if (vcos_is_log_enabled(cat,_level)) vcos_vlog_impl(cat,_level,fmt,ap); } while (0)
# else
#  define _VCOS_LOG_X(cat, _level, fmt...) (void)0
#  define _VCOS_VLOG_X(cat, _level, fmt, ap) (void)0
# endif



# define vcos_log_error(...)   _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_ERROR, __VA_ARGS__)
# define vcos_log_warn(...)    _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_WARN, __VA_ARGS__)
# define vcos_log_info(...)    _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_INFO, __VA_ARGS__)
# define vcos_log_trace(...)   _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE, __VA_ARGS__)

# define vcos_vlog_error(fmt,ap)  _VCOS_VLOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_ERROR, fmt, ap)
# define vcos_vlog_warn(fmt,ap)   _VCOS_VLOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_WARN, fmt, ap)
# define vcos_vlog_info(fmt,ap)   _VCOS_VLOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_INFO, fmt, ap)
# define vcos_vlog_trace(fmt,ap)  _VCOS_VLOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE, fmt, ap)

# define vcos_logc_error(cat,...)   _VCOS_LOG_X(cat, VCOS_LOG_ERROR, __VA_ARGS__)
# define vcos_logc_warn(cat,...)    _VCOS_LOG_X(cat, VCOS_LOG_WARN, __VA_ARGS__)
# define vcos_logc_info(cat,...)    _VCOS_LOG_X(cat, VCOS_LOG_INFO, __VA_ARGS__)
# define vcos_logc_trace(cat,...)   _VCOS_LOG_X(cat, VCOS_LOG_TRACE, __VA_ARGS__)

# define vcos_vlogc_error(cat,fmt,ap)  _VCOS_VLOG_X(cat, VCOS_LOG_ERROR, fmt, ap)
# define vcos_vlogc_warn(cat,fmt,ap)   _VCOS_VLOG_X(cat, VCOS_LOG_WARN, fmt, ap)
# define vcos_vlogc_info(cat,fmt,ap)   _VCOS_VLOG_X(cat, VCOS_LOG_INFO, fmt, ap)
# define vcos_vlogc_trace(cat,fmt,ap)  _VCOS_VLOG_X(cat, VCOS_LOG_TRACE, fmt, ap)

# define vcos_log(...)   _VCOS_LOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_INFO, __VA_ARGS__)
# define vcos_vlog(fmt,ap)  _VCOS_VLOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_INFO, fmt, ap)
# define VCOS_ALERT(...) _VCOS_LOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_ERROR, __VA_ARGS__)
# define VCOS_TRACE(...) _VCOS_LOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_INFO, __VA_ARGS__)

/*
 * MS Visual Studio - pre 2005 does not grok variadic macros
 */
#elif defined(_MSC_VER)

# if _MSC_VER >= 1400

#  if !defined(AMPUTATE_ALL_VCOS_LOGGING) && (!defined(NDEBUG) || defined(VCOS_ALWAYS_WANT_LOGGING))
#   define VCOS_LOGGING_ENABLED
#   define _VCOS_LOG_X(cat, _level, fmt,...) do { if (vcos_is_log_enabled(cat,_level)) vcos_log_impl(cat, _level, fmt, __VA_ARGS__); } while (0)
#  else
#   define _VCOS_LOG_X(cat, _level, fmt,...) (void)0
#  endif

# define vcos_log_error(fmt,...)   _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_ERROR, fmt, __VA_ARGS__)
# define vcos_log_warn(fmt,...)    _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_WARN, fmt, __VA_ARGS__)
# define vcos_log_info(fmt,...)    _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_INFO, fmt, __VA_ARGS__)
# define vcos_log_trace(fmt,...)   _VCOS_LOG_X(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE, fmt, __VA_ARGS__)

# define vcos_logc_error(cat,fmt,...)   _VCOS_LOG_X(cat, VCOS_LOG_ERROR, fmt, __VA_ARGS__)
# define vcos_logc_warn(cat,fmt,...)    _VCOS_LOG_X(cat, VCOS_LOG_WARN, fmt, __VA_ARGS__)
# define vcos_logc_info(cat,fmt,...)    _VCOS_LOG_X(cat, VCOS_LOG_INFO, fmt, __VA_ARGS__)
# define vcos_logc_trace(cat,fmt,...)   _VCOS_LOG_X(cat, VCOS_LOG_TRACE, fmt, __VA_ARGS__)

# define vcos_log(fmt,...)   _VCOS_LOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_INFO, fmt, __VA_ARGS__)
# define VCOS_ALERT(fmt,...) _VCOS_LOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_ERROR, fmt, __VA_ARGS__)
# define VCOS_TRACE(fmt,...) _VCOS_LOG_X(VCOS_LOG_DFLT_CATEGORY, VCOS_LOG_INFO, fmt, __VA_ARGS__)

# else /* _MSC_VER >= 1400 */

/* do not define these */

# endif /* _MSC_VER >= 1400 */

#endif

#if VCOS_HAVE_CMD

#include "interface/vcos/vcos_cmd.h"

/*
 * These are the log sub-commands. They're exported here for user-mode apps which 
 * may want to call these, since the "log" command isn't registered for user-mode 
 * apps (vcdbg for example, has its own log command). 
 */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_log_assert_cmd( VCOS_CMD_PARAM_T *param );
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_log_set_cmd( VCOS_CMD_PARAM_T *param );
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_log_status_cmd( VCOS_CMD_PARAM_T *param );
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_log_test_cmd( VCOS_CMD_PARAM_T *param );
#endif

#ifdef __cplusplus
}
#endif
#endif /* VCOS_LOGGING_H */


