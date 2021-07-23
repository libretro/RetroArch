/**************************************************************

   log.cpp - Simple logging for Switchres

   ---------------------------------------------------------

   Switchres   Modeline generation engine for emulation

   License     GPL-2.0+
   Copyright   2010-2021 Chris Kennedy, Antonio Giner,
                         Alexandre Wodarczyk, Gil Delescluse

 **************************************************************/

#include "log.h"

enum log_verbosity { NONE, SR_ERROR, SR_INFO, SR_DEBUG };
static log_verbosity log_level = SR_INFO;

void log_dummy(const char *, ...) {}

LOG_VERBOSE log_verbose = &log_dummy;
LOG_INFO log_info = &log_dummy;
LOG_ERROR log_error = &log_dummy;

/*
 * These bakup pointers are here to let the user modify the log level at runtime
 * We can't sadly unify a log function and test the log level to test if it should
 * output a log, because it would imply frewriting log_ functions with va_args
 * and wouldn't work with emulators log functions anymore
 */
LOG_VERBOSE log_verbose_bak = &log_dummy;
LOG_INFO log_info_bak = &log_dummy;
LOG_ERROR log_error_bak = &log_dummy;


void set_log_verbose(void *func_ptr)
{
	if (log_level >= SR_DEBUG)
		log_verbose = (LOG_VERBOSE)func_ptr;
	log_verbose_bak = (LOG_VERBOSE)func_ptr;
}

void set_log_info(void *func_ptr)
{
	if (log_level >= SR_INFO)
		log_info = (LOG_INFO)func_ptr;
	log_info_bak = (LOG_INFO)func_ptr;
}

void set_log_error(void *func_ptr)
{
	if (log_level >= SR_ERROR)
		log_error = (LOG_ERROR)func_ptr;
	log_error_bak = (LOG_ERROR)func_ptr;
}

void set_log_verbosity(int level)
{
	// Keep the log in the enum bounds
	if (level < NONE)
		level = NONE;
	if(level > SR_DEBUG)
		level = SR_DEBUG;

	log_error = &log_dummy;
	log_info = &log_dummy;
	log_verbose = &log_dummy;

	if (level >= SR_ERROR)
		log_error = log_error_bak;

	if (level >= SR_INFO)
		log_info = log_info_bak;

	if (level >= SR_DEBUG)
		log_verbose = log_verbose_bak;
}