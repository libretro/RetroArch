/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  debug.h
 *
 *  allow to funnel debug & error messages 
 */

#include "config.h"
#include "hci_dump.h"

#include <stdio.h>

#ifdef ENABLE_LOG_DEBUG
#ifdef HAVE_HCI_DUMP
#define log_debug(format, ...)  hci_dump_log(format,  ## __VA_ARGS__)
#else
#define log_debug(format, ...)  printf(format,  ## __VA_ARGS__)
#endif
#else
#define log_debug(...)
#endif

#ifdef ENABLE_LOG_INFO
#ifdef HAVE_HCI_DUMP
#define log_info(format, ...)  hci_dump_log(format,  ## __VA_ARGS__)
#else
#define log_info(format, ...)  printf(format,  ## __VA_ARGS__)
#endif
#else
#define log_info(...)
#endif

#ifdef ENABLE_LOG_ERROR
#ifdef HAVE_HCI_DUMP
#define log_error(format, ...)  hci_dump_log(format,  ## __VA_ARGS__)
#else
#define log_error(format, ...)  printf(format,  ## __VA_ARGS__)
#endif
#else
#define log_error(...)
#endif
