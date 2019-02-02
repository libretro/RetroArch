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

#ifndef KHRN_CLIENT_RPC_H
#define KHRN_CLIENT_RPC_H

#define RPC_DELAYED_USE_OF_POINTERS

#include "interface/khronos/common/khrn_int_util.h"
#include "interface/khronos/common/khrn_client.h"

#include <stdlib.h> /* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

# if defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
#include "rpc_platform.h"
#endif

#ifdef RPC_DIRECT

#include "middleware/khronos/egl/egl_server.h" /* for egl_server_unlock_states() */
#if !defined(V3D_LEAN) || defined(RPC_DIRECT_MULTI)
#include "middleware/khronos/common/khrn_misc.h"
#endif

#ifdef RPC_LIBRARY
#include "interface/khronos/common/khrn_client.h" /* for khrn_client_get_func_table() */
#include "applications/vmcs/khronos/khronos_server.h"
#endif

#ifdef RPC_DIRECT_MULTI
#include "interface/khronos/common/khrn_client.h" /* for khrn_client_get_func_table() */

extern int client_library_get_connection(void);
extern const KHRONOS_FUNC_TABLE_T *khronos_server_lock_func_table(int);
extern void khronos_server_unlock_func_table(void);

#endif

/******************************************************************************
type packing/unpacking macros
******************************************************************************/

#define RPC_FLOAT(f)        (f)
#define RPC_ENUM(e)         (e)
#define RPC_INT(i)          (i)
#define RPC_INTPTR(p)       (p)
#define RPC_UINT(u)         (u)
#define RPC_SIZEI(s)        (s)
#define RPC_SIZEIPTR(p)     (p)
#define RPC_BOOLEAN(b)      (b)
#define RPC_BITFIELD(b)     (b)
#define RPC_FIXED(f)        (f)
#define RPC_HANDLE(h)       (h)
#define RPC_EGLID(i)        (i)

#if defined(RPC_LIBRARY) || defined(RPC_DIRECT_MULTI)
static INLINE float RPC_FLOAT_RES(float f) { khronos_server_unlock_func_table(); return f; }
static INLINE GLenum RPC_ENUM_RES(GLenum e) { khronos_server_unlock_func_table(); return e; }
static INLINE int RPC_INT_RES(int i) { khronos_server_unlock_func_table(); return i; }
static INLINE uint32_t RPC_UINT_RES(uint32_t u) { khronos_server_unlock_func_table(); return u; }
static INLINE bool RPC_BOOLEAN_RES(bool b) { khronos_server_unlock_func_table(); return b; }
//static INLINE GLbitfield RPC_BITFIELD_RES(GLbitfield b) { khronos_server_unlock_func_table(); return b; }
static INLINE VGHandle RPC_HANDLE_RES(VGHandle h) { khronos_server_unlock_func_table(); return h; }
#else
#define RPC_FLOAT_RES(f)    (f)
#define RPC_ENUM_RES(e)     (e)
#define RPC_INT_RES(i)      (i)
#define RPC_UINT_RES(u)     (u)
#define RPC_BOOLEAN_RES(b)  (b)
#define RPC_BITFIELD_RES(b) (b)
#define RPC_HANDLE_RES(h)   (h)
#endif

/******************************************************************************
rpc call macros
******************************************************************************/

#ifdef RPC_DIRECT_MULTI
extern bool rpc_direct_multi_init(void);
extern void rpc_term(void);
#define RPC_INIT() rpc_direct_multi_init()
#define RPC_TERM() rpc_term()
#else
#define RPC_INIT() true
#define RPC_TERM() 
#endif

#define RPC_FLUSH(thread) RPC_CALL0(khrn_misc_rpc_flush_impl, thread, no_id)
#define RPC_HIGH_PRIORITY_BEGIN(thread)
#define RPC_HIGH_PRIORITY_END(thread)

#if defined(RPC_LIBRARY) || defined(RPC_DIRECT_MULTI)
#define RPC_DO(fn, args) ((khronos_server_lock_func_table(client_library_get_connection())->fn args),khronos_server_unlock_func_table())
#define RPC_DO_RES(fn, args) (khronos_server_lock_func_table(client_library_get_connection())->fn args)
#else
#define RPC_DO(fn, args) fn args
#define RPC_DO_RES(fn, args) fn args
#endif
/*
   RPC_CALL[n](fn, id, RPC_THING(p0), RPC_THING(p1), ...)

   Implementation notes:

   In direct mode, fn is called directly and id is ignored.
   Otherwise fn is ignored and an RPC message is constructed based on id.

   Preconditions:

   p0, p1, etc. satisfy the precondition to fn
   id matches fn
   Server is up (except for a special initialise call)

   Postconditions:

   We promise to call fn on the server at some point in the future. The RPC calls for this
   thread will occur in the same order they are made on the client.
   All postconditions of fn will be satisfied (on the server)

   Invariants preserved:

   -

   Invariants used:

   -
*/
#define RPC_CALL8_MAKECURRENT(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)                                             RPC_CALL8(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)

#define RPC_CALL0(fn, thread, id)                                          RPC_DO(fn, ())
#define RPC_CALL1(fn, thread, id, p0)                                      RPC_DO(fn, (p0))
#define RPC_CALL2(fn, thread, id, p0, p1)                                  RPC_DO(fn, (p0, p1))
#define RPC_CALL3(fn, thread, id, p0, p1, p2)                              RPC_DO(fn, (p0, p1, p2))
#define RPC_CALL4(fn, thread, id, p0, p1, p2, p3)                          RPC_DO(fn, (p0, p1, p2, p3))
#define RPC_CALL5(fn, thread, id, p0, p1, p2, p3, p4)                      RPC_DO(fn, (p0, p1, p2, p3, p4))
#define RPC_CALL6(fn, thread, id, p0, p1, p2, p3, p4, p5)                  RPC_DO(fn, (p0, p1, p2, p3, p4, p5))
#define RPC_CALL7(fn, thread, id, p0, p1, p2, p3, p4, p5, p6)              RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6))
#define RPC_CALL8(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)          RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7))
#define RPC_CALL9(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8)      RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8))
#define RPC_CALL10(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9))
#define RPC_CALL11(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10))
#define RPC_CALL16(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) \
   RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15))

/*
   RPC_THING_RES(RPC_CALL[n]_RES(fn, id, RPC_THING(p0), RPC_THING(p1), ...))

   Implementation notes:

   In direct mode, fn is called directly and id is ignored.
   Otherwise fn is ignored and an RPC message is constructed based on id.

   Preconditions:

   p0, p1, etc. satisfy the precondition to fn
   id matches fn
   Server is up (except for a special initialise call)

   Postconditions:

   The call to fn on the server has completed
   We return the return value of fn, and all postconditions of fn are satisfied (on the server)

   Invariants preserved:

   -

   Invariants used:

   -
*/

#define RPC_CALL0_RES(fn, thread, id)                                                    RPC_DO_RES(fn, ())
#define RPC_CALL1_RES(fn, thread, id, p0)                                                RPC_DO_RES(fn, (p0))
#define RPC_CALL2_RES(fn, thread, id, p0, p1)                                            RPC_DO_RES(fn, (p0, p1))
#define RPC_CALL3_RES(fn, thread, id, p0, p1, p2)                                        RPC_DO_RES(fn, (p0, p1, p2))
#define RPC_CALL4_RES(fn, thread, id, p0, p1, p2, p3)                                    RPC_DO_RES(fn, (p0, p1, p2, p3))
#define RPC_CALL5_RES(fn, thread, id, p0, p1, p2, p3, p4)                                RPC_DO_RES(fn, (p0, p1, p2, p3, p4))
#define RPC_CALL6_RES(fn, thread, id, p0, p1, p2, p3, p4, p5)                            RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5))
#define RPC_CALL7_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6)                        RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6))
#define RPC_CALL8_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)                    RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7))
#define RPC_CALL9_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8)                RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8))
#define RPC_CALL10_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9)           RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9))
#define RPC_CALL11_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)      RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10))
#define RPC_CALL12_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11))

/*
   message with data in/out via control channel
*/

#define RPC_CALL1_IN_CTRL(fn, thread, id, in, len)                                      RPC_DO(fn, (in))
#define RPC_CALL2_IN_CTRL(fn, thread, id, p0, in, len)                                  RPC_DO(fn, (p0, in))
#define RPC_CALL3_IN_CTRL(fn, thread, id, p0, p1, in, len)                              RPC_DO(fn, (p0, p1, in))
#define RPC_CALL4_IN_CTRL(fn, thread, id, p0, p1, p2, in, len)                          RPC_DO(fn, (p0, p1, p2, in))
#define RPC_CALL5_IN_CTRL(fn, thread, id, p0, p1, p2, p3, in, len)                      RPC_DO(fn, (p0, p1, p2, p3, in))
#define RPC_CALL6_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, in, len)                  RPC_DO(fn, (p0, p1, p2, p3, p4, in))
#define RPC_CALL7_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, in, len)              RPC_DO(fn, (p0, p1, p2, p3, p4, p5, in))
#define RPC_CALL8_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)          RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, in))
#define RPC_CALL9_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)      RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, in))
#define RPC_CALL10_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, in))

/*
   RPC_CALL[n]_OUT_CTRL(fn, id, RPC_THING(p0), RPC_THING(p1), ..., out)

   Implementation notes:

   In direct mode, fn is called directly and id is ignored.
   Otherwise fn is ignored and an RPC message is constructed based on id.
   The dispatch code is responsible for calculating the length of the returned data.

   Preconditions:

   p0, p1, ..., out satisfy the precondition to fn
   id matches fn
   Server is up (except for a special initialise call)

   Postconditions:

   The call to fn on the server has completed
   We return whatever fn returned in out

   Invariants preserved:

   -

   Invariants used:

   -
*/

#define RPC_CALL1_OUT_CTRL(fn, thread, id, out)                                               RPC_DO(fn, (out))
#define RPC_CALL2_OUT_CTRL(fn, thread, id, p0, out)                                           RPC_DO(fn, (p0, out))
#define RPC_CALL3_OUT_CTRL(fn, thread, id, p0, p1, out)                                       RPC_DO(fn, (p0, p1, out))
#define RPC_CALL4_OUT_CTRL(fn, thread, id, p0, p1, p2, out)                                   RPC_DO(fn, (p0, p1, p2, out))
#define RPC_CALL5_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, out)                               RPC_DO(fn, (p0, p1, p2, p3, out))
#define RPC_CALL6_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, out)                           RPC_DO(fn, (p0, p1, p2, p3, p4, out))
#define RPC_CALL7_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, out)                       RPC_DO(fn, (p0, p1, p2, p3, p4, p5, out))
#define RPC_CALL8_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)                   RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, out))
#define RPC_CALL9_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)               RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, out))
#define RPC_CALL10_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out)          RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, out))
#define RPC_CALL11_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, out)      RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, out))
#define RPC_CALL12_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, out) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, out))
#define RPC_CALL13_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, out)      RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, out))
#define RPC_CALL14_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, out) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, out))
#define RPC_CALL15_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, out) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, out))

#define RPC_CALL1_OUT_CTRL_RES(fn, thread, id, out)                                      RPC_DO_RES(fn, (out))
#define RPC_CALL2_OUT_CTRL_RES(fn, thread, id, p0, out)                                  RPC_DO_RES(fn, (p0, out))
#define RPC_CALL3_OUT_CTRL_RES(fn, thread, id, p0, p1, out)                              RPC_DO_RES(fn, (p0, p1, out))
#define RPC_CALL4_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, out)                          RPC_DO_RES(fn, (p0, p1, p2, out))
#define RPC_CALL5_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, out)                      RPC_DO_RES(fn, (p0, p1, p2, p3, out))
#define RPC_CALL6_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, out)                  RPC_DO_RES(fn, (p0, p1, p2, p3, p4, out))
#define RPC_CALL7_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, out)              RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, out))
#define RPC_CALL8_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)          RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, out))
#define RPC_CALL9_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)      RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, out))
#define RPC_CALL10_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out) RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, out))

/*
   message with data in/out via bulk channel
*/

#define RPC_CALL1_IN_BULK(fn, thread, id, in, len)                                      RPC_DO(fn, (in))
#define RPC_CALL2_IN_BULK(fn, thread, id, p0, in, len)                                  RPC_DO(fn, (p0, in))
#define RPC_CALL3_IN_BULK(fn, thread, id, p0, p1, in, len)                              RPC_DO(fn, (p0, p1, in))
#define RPC_CALL4_IN_BULK(fn, thread, id, p0, p1, p2, in, len)                          RPC_DO(fn, (p0, p1, p2, in))
#define RPC_CALL5_IN_BULK(fn, thread, id, p0, p1, p2, p3, in, len)                      RPC_DO(fn, (p0, p1, p2, p3, in))
#define RPC_CALL6_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, in, len)                  RPC_DO(fn, (p0, p1, p2, p3, p4, in))
#define RPC_CALL7_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, in, len)              RPC_DO(fn, (p0, p1, p2, p3, p4, p5, in))
#define RPC_CALL8_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)          RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, in))
#define RPC_CALL9_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)      RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, in))
#define RPC_CALL10_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, in))

#define RPC_CALL1_OUT_BULK(fn, thread, id, out)                                      RPC_DO(fn, (out))
#define RPC_CALL2_OUT_BULK(fn, thread, id, p0, out)                                  RPC_DO(fn, (p0, out))
#define RPC_CALL3_OUT_BULK(fn, thread, id, p0, p1, out)                              RPC_DO(fn, (p0, p1, out))
#define RPC_CALL4_OUT_BULK(fn, thread, id, p0, p1, p2, out)                          RPC_DO(fn, (p0, p1, p2, out))
#define RPC_CALL5_OUT_BULK(fn, thread, id, p0, p1, p2, p3, out)                      RPC_DO(fn, (p0, p1, p2, p3, out))
#define RPC_CALL6_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, out)                  RPC_DO(fn, (p0, p1, p2, p3, p4, out))
#define RPC_CALL7_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, out)              RPC_DO(fn, (p0, p1, p2, p3, p4, p5, out))
#define RPC_CALL8_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)          RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, out))
#define RPC_CALL9_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)      RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, out))
#define RPC_CALL10_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out) RPC_DO(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, out))

#define RPC_CALL1_IN_BULK_RES(fn, thread, id, in, len)                                      RPC_DO_RES(fn, (in))
#define RPC_CALL2_IN_BULK_RES(fn, thread, id, p0, in, len)                                  RPC_DO_RES(fn, (p0, in))
#define RPC_CALL3_IN_BULK_RES(fn, thread, id, p0, p1, in, len)                              RPC_DO_RES(fn, (p0, p1, in))
#define RPC_CALL4_IN_BULK_RES(fn, thread, id, p0, p1, p2, in, len)                          RPC_DO_RES(fn, (p0, p1, p2, in))
#define RPC_CALL5_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, in, len)                      RPC_DO_RES(fn, (p0, p1, p2, p3, in))
#define RPC_CALL6_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, in, len)                  RPC_DO_RES(fn, (p0, p1, p2, p3, p4, in))
#define RPC_CALL7_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, in, len)              RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, in))
#define RPC_CALL8_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)          RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, in))
#define RPC_CALL9_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)      RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, in))
#define RPC_CALL10_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len) RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, in))

#define RPC_CALL1_OUT_BULK_RES(fn, thread, id, out)                                      RPC_DO_RES(fn, (out))
#define RPC_CALL2_OUT_BULK_RES(fn, thread, id, p0, out)                                  RPC_DO_RES(fn, (p0, out))
#define RPC_CALL3_OUT_BULK_RES(fn, thread, id, p0, p1, out)                              RPC_DO_RES(fn, (p0, p1, out))
#define RPC_CALL4_OUT_BULK_RES(fn, thread, id, p0, p1, p2, out)                          RPC_DO_RES(fn, (p0, p1, p2, out))
#define RPC_CALL5_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, out)                      RPC_DO_RES(fn, (p0, p1, p2, p3, out))
#define RPC_CALL6_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, out)                  RPC_DO_RES(fn, (p0, p1, p2, p3, p4, out))
#define RPC_CALL7_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, out)              RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, out))
#define RPC_CALL8_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)          RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, out))
#define RPC_CALL9_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)      RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, out))
#define RPC_CALL10_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out) RPC_DO_RES(fn, (p0, p1, p2, p3, p4, p5, p6, p7, p8, out))

#else /* RPC_DIRECT */

#include "interface/khronos/common/khrn_int_ids.h"

#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/VG/openvg.h"

#include <string.h>

/******************************************************************************
core api
******************************************************************************/

extern bool khclient_rpc_init(void);
extern void rpc_term(void);

extern void rpc_flush(CLIENT_THREAD_STATE_T *thread);
extern void rpc_high_priority_begin(CLIENT_THREAD_STATE_T *thread);
extern void rpc_high_priority_end(CLIENT_THREAD_STATE_T *thread);

extern uint64_t rpc_get_client_id(CLIENT_THREAD_STATE_T *thread);

static INLINE uint32_t rpc_pad_ctrl(uint32_t len) { return (len + 0x3) & ~0x3; }
static INLINE uint32_t rpc_pad_bulk(uint32_t len) { return len; }

/* returns the length of the remainder of the merge buffer (after a flush if this length would be < len_min) */
extern uint32_t rpc_send_ctrl_longest(CLIENT_THREAD_STATE_T *thread, uint32_t len_min);

extern void rpc_send_ctrl_begin(CLIENT_THREAD_STATE_T *thread, uint32_t len); /* sum of padded lengths -- use rpc_pad_ctrl */
extern void rpc_send_ctrl_write(CLIENT_THREAD_STATE_T *thread, const uint32_t msg[], uint32_t msglen); /* len bytes read, rpc_pad_ctrl(len) bytes written */
extern void rpc_send_ctrl_end(CLIENT_THREAD_STATE_T *thread);

extern void rpc_send_bulk(CLIENT_THREAD_STATE_T *thread, const void *in, uint32_t len); /* len bytes read, rpc_pad_bulk(len) bytes written. in must remain valid until the next "releasing" rpc_end call */
extern void rpc_send_bulk_gather(CLIENT_THREAD_STATE_T *thread, const void *in, uint32_t len, int32_t stride, uint32_t n); /* n * len bytes read, rpc_pad_bulk(n * len) bytes written */

typedef enum {
   RPC_RECV_FLAG_RES          = 1 << 0,
   RPC_RECV_FLAG_CTRL         = 1 << 1,
   RPC_RECV_FLAG_BULK         = 1 << 2, /* len bytes written, rpc_pad_bulk(len) bytes read */
   RPC_RECV_FLAG_BULK_SCATTER = 1 << 3, /* len = { len, stride, n, first_mask, last_mask }, n * len bytes written, rpc_pad_bulk(n * len) bytes read */
   RPC_RECV_FLAG_LEN          = 1 << 4  /* len provided by other side */
} RPC_RECV_FLAG_T;

extern uint32_t rpc_recv(CLIENT_THREAD_STATE_T *thread, void *out, uint32_t *len, RPC_RECV_FLAG_T flags);
extern void rpc_recv_bulk_gather(CLIENT_THREAD_STATE_T *thread, void *out, uint32_t *len, RPC_RECV_FLAG_T flags); /* n * len bytes read, rpc_pad_bulk(n * len) bytes written */

/*
   all rpc macros and rpc_send_ctrl_begin/rpc_send_ctrl_write/rpc_send_ctrl_end
   are atomic by themselves and do not require calls to rpc_begin/rpc_end

   rpc_begin/rpc_end can be nested, ie the following code will not deadlock:

   rpc_begin(thread);
   rpc_begin(thread);
   rpc_end(thread);
   rpc_end(thread);
*/

extern void rpc_begin(CLIENT_THREAD_STATE_T *thread);
extern void rpc_end(CLIENT_THREAD_STATE_T *thread);


/******************************************************************************
helpers
******************************************************************************/

static INLINE void rpc_gather(void *out, const void *in, uint32_t len, int32_t stride, uint32_t n)
{
   uint32_t i;
   for (i = 0; i != n; ++i) {
      memcpy((uint8_t *)out + (i * len), in, len);
      in = (const uint8_t *)in + stride;
   }
}

static INLINE void rpc_scatter(void *out, uint32_t len, int32_t stride, uint32_t n, uint32_t first_mask, uint32_t last_mask, const void *in)
{
   uint32_t i;
   for (i = 0; i != n; ++i) {
      uint32_t first = 0, last = 0;
      if (first_mask) { first = ((uint8_t *)out)[0] & first_mask; }
      if (last_mask) { last = ((uint8_t *)out)[len - 1] & last_mask; }
      memcpy(out, (const uint8_t *)in + (i * len), len);
      if (first_mask) { ((uint8_t *)out)[0] = (uint8_t)((((uint8_t *)out)[0] & ~first_mask) | first); }
      if (last_mask) { ((uint8_t *)out)[len - 1] = (uint8_t)((((uint8_t *)out)[len - 1] & ~last_mask) | last); }
      out = (uint8_t *)out + stride;
   }
}

/******************************************************************************
type packing/unpacking macros
******************************************************************************/

#define RPC_FLOAT(f)        (float_to_bits(f))
#define RPC_ENUM(e)         ((uint32_t)(e))
#define RPC_INT(i)          ((uint32_t)(i))
#define RPC_INTPTR(p)       ((uint32_t)(p))
#define RPC_UINT(u)         ((uint32_t)(u))
#define RPC_SIZEI(s)        ((uint32_t)(s))
#define RPC_SIZEIPTR(p)     ((uint32_t)(p))
#define RPC_BOOLEAN(b)      ((uint32_t)(b))
#define RPC_BITFIELD(b)     ((uint32_t)(b))
#define RPC_FIXED(f)        ((uint32_t)(f))
#define RPC_HANDLE(h)       ((uint32_t)(h))
#define RPC_EGLID(i)        ((uint32_t)(size_t)(i))

#define RPC_FLOAT_RES(f)    (float_from_bits(f))
#define RPC_ENUM_RES(e)     ((GLenum)(e))
#define RPC_INT_RES(i)      ((GLint)(i))
#define RPC_UINT_RES(u)     ((GLuint)(u))
#define RPC_BOOLEAN_RES(b)  (!!(b))
#define RPC_BITFIELD_RES(b) ((GLbitfield)(b))
#define RPC_HANDLE_RES(h)   ((VGHandle)(h))

/******************************************************************************
rpc call macros
******************************************************************************/

#define RPC_INIT() khclient_rpc_init()
#define RPC_TERM() rpc_term()

#define RPC_FLUSH(thread) rpc_flush(thread)
#define RPC_HIGH_PRIORITY_BEGIN(thread) rpc_high_priority_begin(thread)
#define RPC_HIGH_PRIORITY_END(thread) rpc_high_priority_end(thread)

#define RPC_CALL8_MAKECURRENT(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)                                             rpc_call8_makecurrent(thread, id, p0, p1, p2, p3, p4, p5, p6, p7)
void rpc_call8_makecurrent(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7);
uint32_t khronos_kernel_semaphore_create( uint32_t name, uint32_t count );
uint32_t khronos_kernel_semaphore_acquire( uint32_t semaphore );
uint32_t khronos_kernel_semaphore_acquire_and_release( uint32_t semaphore );
uint32_t khronos_kernel_semaphore_release( uint32_t semaphore );
uint32_t khronos_kernel_semaphore_destroy( uint32_t semaphore );

/*
   helper macros (shouldn't be used directly)
*/

# if defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
#else
#ifdef __HIGHC__
   /*
      use XXX.../XXX syntax for variadic macros
   */

   #define RPC_CALL(thread, ARGS...) \
      do { \
         uint32_t message_[] = { ARGS }; \
         rpc_send_ctrl_begin(thread, sizeof(message_)); \
         rpc_send_ctrl_write(thread, message_, sizeof(message_)); \
         rpc_send_ctrl_end(thread); \
      } while (0)

   #define RPC_CALL_IN_CTRL(thread, IN, LEN, ARGS...) \
      do { \
         const void *in_ = IN; \
         uint32_t len_ = LEN; \
         uint32_t message_[] = { ARGS }; \
         rpc_send_ctrl_begin(thread, sizeof(message_) + rpc_pad_ctrl(len_)); \
         rpc_send_ctrl_write(thread, message_, sizeof(message_)); \
         rpc_send_ctrl_write(thread, (uint32_t *)in_, len_); \
         rpc_send_ctrl_end(thread); \
      } while (0)

   #define RPC_CALL_IN_BULK(thread, IN, LEN, ARGS...) \
      do { \
         const void *in_ = IN; \
         uint32_t len_ = LEN; \
         uint32_t message_[] = { ARGS, in_ ? len_ : LENGTH_SIGNAL_NULL }; \
         rpc_send_ctrl_begin(thread, sizeof(message_)); \
         rpc_send_ctrl_write(thread, message_, sizeof(message_)); \
         rpc_send_ctrl_end(thread); \
         rpc_send_bulk(thread, in_, len_); \
      } while (0)
#else
   /*
      use c99 .../__VA_ARGS__ syntax for variadic macros
   */

   #define RPC_CALL(thread, ...) \
      do { \
         uint32_t message_[] = { __VA_ARGS__ }; \
         rpc_send_ctrl_begin(thread, sizeof(message_)); \
         rpc_send_ctrl_write(thread, message_, sizeof(message_)); \
         rpc_send_ctrl_end(thread); \
      } while (0)

   #define RPC_CALL_IN_CTRL(thread, IN, LEN, ...) \
      do { \
         const void *in_ = IN; \
         uint32_t len_ = LEN; \
         uint32_t message_[] = { __VA_ARGS__ }; \
         rpc_send_ctrl_begin(thread, sizeof(message_) + rpc_pad_ctrl(len_)); \
         rpc_send_ctrl_write(thread, message_, sizeof(message_)); \
         rpc_send_ctrl_write(thread, in_, len_); \
         rpc_send_ctrl_end(thread); \
      } while (0)

   #define RPC_CALL_IN_BULK(thread, IN, LEN, ...) \
      do { \
         const void *in_ = IN; \
         uint32_t len_ = LEN; \
         uint32_t message_[] = { __VA_ARGS__, in_ ? len_ : LENGTH_SIGNAL_NULL }; \
         rpc_send_ctrl_begin(thread, sizeof(message_)); \
         rpc_send_ctrl_write(thread, message_, sizeof(message_)); \
         rpc_send_ctrl_end(thread); \
         rpc_send_bulk(thread, in_, len_); \
      } while (0)

#endif
#endif

/*
   just message
*/

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE void rpc_call0(CLIENT_THREAD_STATE_T *thread, uint32_t id)                                                                                                                                    { RPC_CALL(thread, id);                                         }
static INLINE void rpc_call1(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0)                                                                                                                       { RPC_CALL(thread, id, p0);                                     }
static INLINE void rpc_call2(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1)                                                                                                          { RPC_CALL(thread, id, p0, p1);                                 }
static INLINE void rpc_call3(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2)                                                                                             { RPC_CALL(thread, id, p0, p1, p2);                             }
static INLINE void rpc_call4(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)                                                                                { RPC_CALL(thread, id, p0, p1, p2, p3);                         }
static INLINE void rpc_call5(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)                                                                   { RPC_CALL(thread, id, p0, p1, p2, p3, p4);                     }
static INLINE void rpc_call6(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)                                                      { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5);                 }
static INLINE void rpc_call7(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6)                                         { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6);             }
static INLINE void rpc_call8(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7)                            { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7);         }
static INLINE void rpc_call9(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8)               { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8);     }
static INLINE void rpc_call10(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9) { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9); }
static INLINE void rpc_call16(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7,
   uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, uint32_t p12, uint32_t p13, uint32_t p14, uint32_t p15)
   { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15); }
static INLINE void rpc_call18(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7,
   uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, uint32_t p12, uint32_t p13, uint32_t p14, uint32_t p15, uint32_t p16, uint32_t p17)
   { RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17); }

#endif

#define RPC_CALL0(fn, thread, id)                                          rpc_call0(thread, id)
#define RPC_CALL1(fn, thread, id, p0)                                      rpc_call1(thread, id, p0)
#define RPC_CALL2(fn, thread, id, p0, p1)                                  rpc_call2(thread, id, p0, p1)
#define RPC_CALL3(fn, thread, id, p0, p1, p2)                              rpc_call3(thread, id, p0, p1, p2)
#define RPC_CALL4(fn, thread, id, p0, p1, p2, p3)                          rpc_call4(thread, id, p0, p1, p2, p3)
#define RPC_CALL5(fn, thread, id, p0, p1, p2, p3, p4)                      rpc_call5(thread, id, p0, p1, p2, p3, p4)
#define RPC_CALL6(fn, thread, id, p0, p1, p2, p3, p4, p5)                  rpc_call6(thread, id, p0, p1, p2, p3, p4, p5)
#define RPC_CALL7(fn, thread, id, p0, p1, p2, p3, p4, p5, p6)              rpc_call7(thread, id, p0, p1, p2, p3, p4, p5, p6)
#define RPC_CALL8(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)          rpc_call8(thread, id, p0, p1, p2, p3, p4, p5, p6, p7)
#define RPC_CALL9(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8)      rpc_call9(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8)
#define RPC_CALL10(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9) rpc_call10(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define RPC_CALL11(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10) rpc_call10(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define RPC_CALL16(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15) \
   rpc_call16(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15)
#define RPC_CALL18(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17) \
   rpc_call18(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17)

/*
   RPC_THING_RES(rpc_call[n]_res(id, RPC_THING(p0), RPC_THING(p1), ...))

   Implementation notes:

   -

   Preconditions:

   Not in RPC_DIRECT mode
   p0, p1, etc. satisfy the preconditions of the operation identified by id
   Server is up (except for a special initialise call)

   Postconditions:

   The operation identified by id has completed on the server
   We return the result of this operation, and all postconditions are satisfied (on the server)

   Invariants preserved:

   -

   Invariants used:

   -
*/

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE uint32_t rpc_call0_res(CLIENT_THREAD_STATE_T *thread, uint32_t id)                                                                                                                                                                { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id);                                                   res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call1_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0)                                                                                                                                                   { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0);                                               res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call2_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1)                                                                                                                                      { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1);                                           res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call3_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2)                                                                                                                         { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2);                                       res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call4_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)                                                                                                            { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3);                                   res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call5_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)                                                                                               { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4);                               res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call6_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)                                                                                  { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5);                           res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call7_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6)                                                                     { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6);                       res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call8_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7)                                                        { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7);                   res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call9_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8)                                           { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8);               res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call10_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9)                             { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9);           res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call11_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10)               { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);      res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call12_res(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11) { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
#endif

#define RPC_CALL0_RES(fn, thread, id)                                                    rpc_call0_res(thread, id)
#define RPC_CALL1_RES(fn, thread, id, p0)                                                rpc_call1_res(thread, id, p0)
#define RPC_CALL2_RES(fn, thread, id, p0, p1)                                            rpc_call2_res(thread, id, p0, p1)
#define RPC_CALL3_RES(fn, thread, id, p0, p1, p2)                                        rpc_call3_res(thread, id, p0, p1, p2)
#define RPC_CALL4_RES(fn, thread, id, p0, p1, p2, p3)                                    rpc_call4_res(thread, id, p0, p1, p2, p3)
#define RPC_CALL5_RES(fn, thread, id, p0, p1, p2, p3, p4)                                rpc_call5_res(thread, id, p0, p1, p2, p3, p4)
#define RPC_CALL6_RES(fn, thread, id, p0, p1, p2, p3, p4, p5)                            rpc_call6_res(thread, id, p0, p1, p2, p3, p4, p5)
#define RPC_CALL7_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6)                        rpc_call7_res(thread, id, p0, p1, p2, p3, p4, p5, p6)
#define RPC_CALL8_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7)                    rpc_call8_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7)
#define RPC_CALL9_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8)                rpc_call9_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8)
#define RPC_CALL10_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9)           rpc_call10_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9)
#define RPC_CALL11_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)      rpc_call11_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
#define RPC_CALL12_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11) rpc_call12_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11)

/*
   message with data in/out via control channel
*/

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE void rpc_call1_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, const void *in, uint32_t len)                                                                                                                       { RPC_CALL_IN_CTRL(thread, in, len, id);                                     }
static INLINE void rpc_call2_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, const void *in, uint32_t len)                                                                                                          { RPC_CALL_IN_CTRL(thread, in, len, id, p0);                                 }
static INLINE void rpc_call3_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, const void *in, uint32_t len)                                                                                             { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1);                             }
static INLINE void rpc_call4_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, const void *in, uint32_t len)                                                                                { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2);                         }
static INLINE void rpc_call5_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const void *in, uint32_t len)                                                                   { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2, p3);                     }
static INLINE void rpc_call6_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, const void *in, uint32_t len)                                                      { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2, p3, p4);                 }
static INLINE void rpc_call7_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, const void *in, uint32_t len)                                         { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2, p3, p4, p5);             }
static INLINE void rpc_call8_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, const void *in, uint32_t len)                            { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6);         }
static INLINE void rpc_call9_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, const void *in, uint32_t len)               { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6, p7);     }
static INLINE void rpc_call10_in_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, const void *in, uint32_t len) { RPC_CALL_IN_CTRL(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6, p7, p8); }

#endif

#define RPC_CALL1_IN_CTRL(fn, thread, id, in, len)                                      rpc_call1_in_ctrl(thread, id, in, len)
#define RPC_CALL2_IN_CTRL(fn, thread, id, p0, in, len)                                  rpc_call2_in_ctrl(thread, id, p0, in, len)
#define RPC_CALL3_IN_CTRL(fn, thread, id, p0, p1, in, len)                              rpc_call3_in_ctrl(thread, id, p0, p1, in, len)
#define RPC_CALL4_IN_CTRL(fn, thread, id, p0, p1, p2, in, len)                          rpc_call4_in_ctrl(thread, id, p0, p1, p2, in, len)
#define RPC_CALL5_IN_CTRL(fn, thread, id, p0, p1, p2, p3, in, len)                      rpc_call5_in_ctrl(thread, id, p0, p1, p2, p3, in, len)
#define RPC_CALL6_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, in, len)                  rpc_call6_in_ctrl(thread, id, p0, p1, p2, p3, p4, in, len)
#define RPC_CALL7_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, in, len)              rpc_call7_in_ctrl(thread, id, p0, p1, p2, p3, p4, p5, in, len)
#define RPC_CALL8_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)          rpc_call8_in_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)
#define RPC_CALL9_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)      rpc_call9_in_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)
#define RPC_CALL10_IN_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len) rpc_call10_in_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len)

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE void rpc_call1_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, void *out)                                                                                                                                                  { rpc_begin(thread); RPC_CALL(thread, id);                                              rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call2_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, void *out)                                                                                                                                     { rpc_begin(thread); RPC_CALL(thread, id, p0);                                          rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call3_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, void *out)                                                                                                                        { rpc_begin(thread); RPC_CALL(thread, id, p0, p1);                                      rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call4_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, void *out)                                                                                                           { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2);                                  rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call5_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, void *out)                                                                                              { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3);                              rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call6_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, void *out)                                                                                 { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4);                          rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call7_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, void *out)                                                                    { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5);                      rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call8_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, void *out)                                                       { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6);                  rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call9_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, void *out)                                          { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7);              rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call10_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, void *out)                            { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8);          rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call11_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, void *out)               { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9);      rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call12_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, void *out) { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call13_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, void *out)               { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11);      rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call14_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, uint32_t p12, void *out) { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call15_out_ctrl(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, uint32_t p12, uint32_t p13, void *out) { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
#endif

#define RPC_CALL1_OUT_CTRL(fn, thread, id, out)                                               rpc_call1_out_ctrl(thread, id, out)
#define RPC_CALL2_OUT_CTRL(fn, thread, id, p0, out)                                           rpc_call2_out_ctrl(thread, id, p0, out)
#define RPC_CALL3_OUT_CTRL(fn, thread, id, p0, p1, out)                                       rpc_call3_out_ctrl(thread, id, p0, p1, out)
#define RPC_CALL4_OUT_CTRL(fn, thread, id, p0, p1, p2, out)                                   rpc_call4_out_ctrl(thread, id, p0, p1, p2, out)
#define RPC_CALL5_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, out)                               rpc_call5_out_ctrl(thread, id, p0, p1, p2, p3, out)
#define RPC_CALL6_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, out)                           rpc_call6_out_ctrl(thread, id, p0, p1, p2, p3, p4, out)
#define RPC_CALL7_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, out)                       rpc_call7_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, out)
#define RPC_CALL8_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)                   rpc_call8_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, out)
#define RPC_CALL9_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)               rpc_call9_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)
#define RPC_CALL10_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out)          rpc_call10_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out)
#define RPC_CALL11_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, out)      rpc_call11_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, out)
#define RPC_CALL12_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, out) rpc_call12_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, out)
#define RPC_CALL13_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, out)      rpc_call13_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, out)
#define RPC_CALL14_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, out) rpc_call14_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, out)
#define RPC_CALL15_OUT_CTRL(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, out) rpc_call15_out_ctrl(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, out)

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE uint32_t rpc_call1_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, void *out)                                                                                                                       { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id);                                     res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call2_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, void *out)                                                                                                          { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0);                                 res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call3_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, void *out)                                                                                             { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1);                             res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call4_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, void *out)                                                                                { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2);                         res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call5_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, void *out)                                                                   { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3);                     res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call6_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, void *out)                                                      { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4);                 res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call7_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, void *out)                                         { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5);             res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call8_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, void *out)                            { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6);         res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call9_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, void *out)               { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7);     res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call10_out_ctrl_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, void *out) { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8); res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
#endif

#define RPC_CALL1_OUT_CTRL_RES(fn, thread, id, out)                                      rpc_call1_out_ctrl_res(thread, id, out)
#define RPC_CALL2_OUT_CTRL_RES(fn, thread, id, p0, out)                                  rpc_call2_out_ctrl_res(thread, id, p0, out)
#define RPC_CALL3_OUT_CTRL_RES(fn, thread, id, p0, p1, out)                              rpc_call3_out_ctrl_res(thread, id, p0, p1, out)
#define RPC_CALL4_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, out)                          rpc_call4_out_ctrl_res(thread, id, p0, p1, p2, out)
#define RPC_CALL5_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, out)                      rpc_call5_out_ctrl_res(thread, id, p0, p1, p2, p3, out)
#define RPC_CALL6_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, out)                  rpc_call6_out_ctrl_res(thread, id, p0, p1, p2, p3, p4, out)
#define RPC_CALL7_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, out)              rpc_call7_out_ctrl_res(thread, id, p0, p1, p2, p3, p4, p5, out)
#define RPC_CALL8_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)          rpc_call8_out_ctrl_res(thread, id, p0, p1, p2, p3, p4, p5, p6, out)
#define RPC_CALL9_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)      rpc_call9_out_ctrl_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)
#define RPC_CALL10_OUT_CTRL_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out) rpc_call10_out_ctrl_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out)

/*
   message with data in/out via bulk channel

   if in is NULL, no bulk transfer will be performed and LENGTH_SIGNAL_NULL will
   be passed instead of len
*/

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE void rpc_call1_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, const void *in, uint32_t len)                                                                                                                       { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id);                                     rpc_end(thread); }
static INLINE void rpc_call2_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, const void *in, uint32_t len)                                                                                                          { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0);                                 rpc_end(thread); }
static INLINE void rpc_call3_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, const void *in, uint32_t len)                                                                                             { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1);                             rpc_end(thread); }
static INLINE void rpc_call4_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, const void *in, uint32_t len)                                                                                { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2);                         rpc_end(thread); }
static INLINE void rpc_call5_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const void *in, uint32_t len)                                                                   { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3);                     rpc_end(thread); }
static INLINE void rpc_call6_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, const void *in, uint32_t len)                                                      { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4);                 rpc_end(thread); }
static INLINE void rpc_call7_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, const void *in, uint32_t len)                                         { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5);             rpc_end(thread); }
static INLINE void rpc_call8_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, const void *in, uint32_t len)                            { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6);         rpc_end(thread); }
static INLINE void rpc_call9_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, const void *in, uint32_t len)               { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6, p7);     rpc_end(thread); }
static INLINE void rpc_call10_in_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, const void *in, uint32_t len) { rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6, p7, p8); rpc_end(thread); }
#endif

#define RPC_CALL1_IN_BULK(fn, thread, id, in, len)                                      rpc_call1_in_bulk(thread, id, in, len)
#define RPC_CALL2_IN_BULK(fn, thread, id, p0, in, len)                                  rpc_call2_in_bulk(thread, id, p0, in, len)
#define RPC_CALL3_IN_BULK(fn, thread, id, p0, p1, in, len)                              rpc_call3_in_bulk(thread, id, p0, p1, in, len)
#define RPC_CALL4_IN_BULK(fn, thread, id, p0, p1, p2, in, len)                          rpc_call4_in_bulk(thread, id, p0, p1, p2, in, len)
#define RPC_CALL5_IN_BULK(fn, thread, id, p0, p1, p2, p3, in, len)                      rpc_call5_in_bulk(thread, id, p0, p1, p2, p3, in, len)
#define RPC_CALL6_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, in, len)                  rpc_call6_in_bulk(thread, id, p0, p1, p2, p3, p4, in, len)
#define RPC_CALL7_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, in, len)              rpc_call7_in_bulk(thread, id, p0, p1, p2, p3, p4, p5, in, len)
#define RPC_CALL8_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)          rpc_call8_in_bulk(thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)
#define RPC_CALL9_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)      rpc_call9_in_bulk(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)
#define RPC_CALL10_IN_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len) rpc_call10_in_bulk(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len)

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE void rpc_call1_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, void *out)                                                                                                                       { rpc_begin(thread); RPC_CALL(thread, id);                                     rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call2_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, void *out)                                                                                                          { rpc_begin(thread); RPC_CALL(thread, id, p0);                                 rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call3_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, void *out)                                                                                             { rpc_begin(thread); RPC_CALL(thread, id, p0, p1);                             rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call4_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, void *out)                                                                                { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2);                         rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call5_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, void *out)                                                                   { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3);                     rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call6_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, void *out)                                                      { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4);                 rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call7_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, void *out)                                         { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5);             rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call8_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, void *out)                            { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6);         rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call9_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, void *out)               { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7);     rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
static INLINE void rpc_call10_out_bulk(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, void *out) { rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8); rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); }
#endif

#define RPC_CALL1_OUT_BULK(fn, thread, id, out)                                      rpc_call1_out_bulk(thread, id, out)
#define RPC_CALL2_OUT_BULK(fn, thread, id, p0, out)                                  rpc_call2_out_bulk(thread, id, p0, out)
#define RPC_CALL3_OUT_BULK(fn, thread, id, p0, p1, out)                              rpc_call3_out_bulk(thread, id, p0, p1, out)
#define RPC_CALL4_OUT_BULK(fn, thread, id, p0, p1, p2, out)                          rpc_call4_out_bulk(thread, id, p0, p1, p2, out)
#define RPC_CALL5_OUT_BULK(fn, thread, id, p0, p1, p2, p3, out)                      rpc_call5_out_bulk(thread, id, p0, p1, p2, p3, out)
#define RPC_CALL6_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, out)                  rpc_call6_out_bulk(thread, id, p0, p1, p2, p3, p4, out)
#define RPC_CALL7_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, out)              rpc_call7_out_bulk(thread, id, p0, p1, p2, p3, p4, p5, out)
#define RPC_CALL8_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)          rpc_call8_out_bulk(thread, id, p0, p1, p2, p3, p4, p5, p6, out)
#define RPC_CALL9_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)      rpc_call9_out_bulk(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)
#define RPC_CALL10_OUT_BULK(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out) rpc_call10_out_bulk(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out)

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE uint32_t rpc_call1_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, const void *in, uint32_t len)                                                                                                                       { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id);                                     res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call2_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, const void *in, uint32_t len)                                                                                                          { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0);                                 res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call3_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, const void *in, uint32_t len)                                                                                             { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1);                             res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call4_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, const void *in, uint32_t len)                                                                                { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2);                         res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call5_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, const void *in, uint32_t len)                                                                   { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3);                     res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call6_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, const void *in, uint32_t len)                                                      { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4);                 res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call7_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, const void *in, uint32_t len)                                         { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5);             res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call8_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, const void *in, uint32_t len)                            { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6);         res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call9_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, const void *in, uint32_t len)               { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6, p7);     res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call10_in_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, const void *in, uint32_t len) { uint32_t res; rpc_begin(thread); RPC_CALL_IN_BULK(thread, in, len, id, p0, p1, p2, p3, p4, p5, p6, p7, p8); res = rpc_recv(thread, NULL, NULL, RPC_RECV_FLAG_RES); rpc_end(thread); return res; }
#endif

#define RPC_CALL1_IN_BULK_RES(fn, thread, id, in, len)                                      rpc_call1_in_bulk_res(thread, id, in, len)
#define RPC_CALL2_IN_BULK_RES(fn, thread, id, p0, in, len)                                  rpc_call2_in_bulk_res(thread, id, p0, in, len)
#define RPC_CALL3_IN_BULK_RES(fn, thread, id, p0, p1, in, len)                              rpc_call3_in_bulk_res(thread, id, p0, p1, in, len)
#define RPC_CALL4_IN_BULK_RES(fn, thread, id, p0, p1, p2, in, len)                          rpc_call4_in_bulk_res(thread, id, p0, p1, p2, in, len)
#define RPC_CALL5_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, in, len)                      rpc_call5_in_bulk_res(thread, id, p0, p1, p2, p3, in, len)
#define RPC_CALL6_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, in, len)                  rpc_call6_in_bulk_res(thread, id, p0, p1, p2, p3, p4, in, len)
#define RPC_CALL7_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, in, len)              rpc_call7_in_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, in, len)
#define RPC_CALL8_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)          rpc_call8_in_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, p6, in, len)
#define RPC_CALL9_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)      rpc_call9_in_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, in, len)
#define RPC_CALL10_IN_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len) rpc_call10_in_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, in, len)

# if !defined(__SYMBIAN32__)  //use functions defined in khrpc.cpp
static INLINE uint32_t rpc_call1_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, void *out)                                                                                                                       { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id);                                     res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call2_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, void *out)                                                                                                          { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0);                                 res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call3_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, void *out)                                                                                             { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1);                             res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call4_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, void *out)                                                                                { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2);                         res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call5_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, void *out)                                                                   { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3);                     res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call6_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, void *out)                                                      { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4);                 res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call7_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, void *out)                                         { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5);             res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call8_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, void *out)                            { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6);         res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call9_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, void *out)               { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7);     res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
static INLINE uint32_t rpc_call10_out_bulk_res(CLIENT_THREAD_STATE_T *thread,uint32_t id, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7, uint32_t p8, void *out) { uint32_t res; rpc_begin(thread); RPC_CALL(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8); res = rpc_recv(thread, out, NULL, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_RES | RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_LEN)); rpc_end(thread); return res; }
#endif

#define RPC_CALL1_OUT_BULK_RES(fn, thread, id, out)                                      rpc_call1_out_bulk_res(thread, id, out)
#define RPC_CALL2_OUT_BULK_RES(fn, thread, id, p0, out)                                  rpc_call2_out_bulk_res(thread, id, p0, out)
#define RPC_CALL3_OUT_BULK_RES(fn, thread, id, p0, p1, out)                              rpc_call3_out_bulk_res(thread, id, p0, p1, out)
#define RPC_CALL4_OUT_BULK_RES(fn, thread, id, p0, p1, p2, out)                          rpc_call4_out_bulk_res(thread, id, p0, p1, p2, out)
#define RPC_CALL5_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, out)                      rpc_call5_out_bulk_res(thread, id, p0, p1, p2, p3, out)
#define RPC_CALL6_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, out)                  rpc_call6_out_bulk_res(thread, id, p0, p1, p2, p3, p4, out)
#define RPC_CALL7_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, out)              rpc_call7_out_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, out)
#define RPC_CALL8_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, out)          rpc_call8_out_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, p6, out)
#define RPC_CALL9_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)      rpc_call9_out_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, out)
#define RPC_CALL10_OUT_BULK_RES(fn, thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out) rpc_call10_out_bulk_res(thread, id, p0, p1, p2, p3, p4, p5, p6, p7, p8, out)

#endif /* RPC_DIRECT */

#ifdef __cplusplus
 }
#endif
#endif
