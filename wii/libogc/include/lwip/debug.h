/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIP_DEBUG_H__
#define __LWIP_DEBUG_H__

#include "arch/cc.h"

/** lower two bits indicate debug level
 * - 0 off
 * - 1 warning
 * - 2 serious
 * - 3 severe
 */

#define DBG_LEVEL_OFF     0
#define DBG_LEVEL_WARNING 1  /* bad checksums, dropped packets, ... */
#define DBG_LEVEL_SERIOUS 2  /* memory allocation failures, ... */
#define DBG_LEVEL_SEVERE  3  /* */
#define DBG_MASK_LEVEL    3

/** flag for LWIP_DEBUGF to enable that debug message */
#define DBG_ON  0x80U
/** flag for LWIP_DEBUGF to disable that debug message */
#define DBG_OFF 0x00U

/** flag for LWIP_DEBUGF indicating a tracing message (to follow program flow) */
#define DBG_TRACE   0x40U
/** flag for LWIP_DEBUGF indicating a state debug message (to follow module states) */
#define DBG_STATE   0x20U
/** flag for LWIP_DEBUGF indicating newly added code, not thoroughly tested yet */
#define DBG_FRESH   0x10U
/** flag for LWIP_DEBUGF to halt after printing this debug message */
#define DBG_HALT    0x08U

#ifdef LWIP_DEBUG
# ifndef LWIP_NOASSERT
#  define LWIP_ASSERT(x,y) do { if(!(y)) LWIP_PLATFORM_ASSERT(x); } while(0)
# else
#  define LWIP_ASSERT(x,y)
# endif
#endif

#ifdef LWIP_DEBUG
/** print debug message only if debug message type is enabled...
 *  AND is of correct type AND is at least DBG_LEVEL
 */
#  define LWIP_DEBUGF(debug,x) do { if (((debug) & DBG_ON) && ((debug) & DBG_TYPES_ON) && ((int)((debug) & DBG_MASK_LEVEL) >= DBG_MIN_LEVEL)) { LWIP_PLATFORM_DIAG(x); if ((debug) & DBG_HALT) while(1); } } while(0)
#  define LWIP_ERROR(x)   do { LWIP_PLATFORM_DIAG(x); } while(0)
#else /* LWIP_DEBUG */
#  define LWIP_ASSERT(x,y)
#  define LWIP_DEBUGF(debug,x)
#  define LWIP_ERROR(x)
#endif /* LWIP_DEBUG */

#endif /* __LWIP_DEBUG_H__ */
