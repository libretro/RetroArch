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

#ifndef MMAL_LOGGING_H
#define MMAL_LOGGING_H

#include "mmal_common.h"
#include "interface/vcos/vcos_logging.h"

#ifndef VCOS_LOG_CATEGORY
#define VCOS_LOG_CATEGORY (&mmal_log_category)
extern VCOS_LOG_CAT_T mmal_log_category;
#endif

#if defined(__GNUC__) && (( __GNUC__ > 2 ) || (( __GNUC__ == 2 ) && ( __GNUC_MINOR__ >= 3 )))
#define mmal_log_error(s, ...) vcos_log_error("%s: " s, VCOS_FUNCTION, ## __VA_ARGS__)
#define mmal_log_info(s, ...) vcos_log_info("%s: " s, VCOS_FUNCTION, ## __VA_ARGS__)
#define mmal_log_warn(s, ...) vcos_log_warn("%s: " s, VCOS_FUNCTION, ## __VA_ARGS__)
#define mmal_log_debug(s, ...) vcos_log_info("%s: " s, VCOS_FUNCTION, ## __VA_ARGS__)
#define mmal_log_trace(s, ...) vcos_log_trace("%s: " s, VCOS_FUNCTION, ## __VA_ARGS__)
#elif defined(_MSC_VER)
#define mmal_log_error(s, ...) vcos_log_error("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_info(s, ...) vcos_log_info("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_warn(s, ...) vcos_log_warn("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_debug(s, ...) vcos_log_info("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_trace(s, ...) vcos_log_trace("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#else
#define mmal_log_error_fun(s, ...) vcos_log_error("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_info_fun(s, ...) vcos_log_info("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_warn_fun(s, ...) vcos_log_warn("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_debug_fun(s, ...) vcos_log_info("%s: " s, VCOS_FUNCTION, __VA_ARGS__)
#define mmal_log_trace_fun(s, ...) vcos_log_trace("%s: " s, VCOS_FUNCTION, __VA_ARGS__)

#define mmal_log_error(s...) mmal_log_error_fun(s, 0)
#define mmal_log_info(s...) mmal_log_info_fun(s, 0)
#define mmal_log_warn(s...)  mmal_log_warn_fun(s, 0)
#define mmal_log_debug(s...)  mmal_log_debug_fun(s, 0)
#define mmal_log_trace(s...) mmal_log_trace_fun(s, 0)
#endif

#define LOG_ERROR mmal_log_error
#define LOG_INFO mmal_log_info
#define LOG_WARN mmal_log_warn
#define LOG_DEBUG mmal_log_debug
#define LOG_TRACE mmal_log_trace

#endif /* MMAL_LOGGING_H */
