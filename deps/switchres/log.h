/**************************************************************

   log.h - Simple logging for Switchres

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#ifndef __LOG__
#define __LOG__

#if defined(__GNUC__)
#define ATTR_PRINTF(x,y)        __attribute__((format(printf, x, y)))
#else
#define ATTR_PRINTF(x,y)
#endif

typedef void (*LOG_VERBOSE)(const char *format, ...) ATTR_PRINTF(1,2);
extern LOG_VERBOSE log_verbose;

typedef void (*LOG_INFO)(const char *format, ...) ATTR_PRINTF(1,2);
extern LOG_INFO log_info;

typedef void (*LOG_ERROR)(const char *format, ...) ATTR_PRINTF(1,2);
extern LOG_ERROR log_error;

void set_log_verbosity(int);
void set_log_verbose(void *func_ptr);
void set_log_info(void *func_ptr);
void set_log_error(void *func_ptr);

#endif
