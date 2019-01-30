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

#ifndef V3D_VER_H
#define V3D_VER_H

/* if V3D_TECH_VERSION/V3D_REVISION aren't defined, try to figure them out from
 * other defines... */
#ifndef V3D_TECH_VERSION
   #define V3D_TECH_VERSION 2
#endif
#ifndef V3D_REVISION
   #ifdef __BCM2708A0__
      #ifdef HERA
         /* bcg a1, hera */
         #define V3D_REVISION 1
      #else
         /* 2708 a0 */
         #define V3D_REVISION 0
      #endif
   #elif defined(__BCM2708B0__)
      /* 2708 b0 */
      #define V3D_REVISION 2
   #elif defined(__BCM2708C0__) || defined(__BCM2708C1__)
      /* 2708 c0/1 */
      #define V3D_REVISION 3
   #else
      /* capri */
      #define V3D_REVISION 6
   #endif
#endif

#define V3D_MAKE_VER(TECH_VERSION, REVISION) (((TECH_VERSION) * 100) + (REVISION))
#define V3D_VER (V3D_MAKE_VER(V3D_TECH_VERSION, V3D_REVISION))
#define V3D_VER_AT_LEAST(TECH_VERSION, REVISION) (V3D_VER >= V3D_MAKE_VER(TECH_VERSION, REVISION))

/* TODO this is temporary */
#if V3D_VER == V3D_MAKE_VER(9, 9)
#define V3D_VER_D3D
#endif

#endif
