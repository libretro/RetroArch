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

#ifndef VCORE_H
#define VCORE_H

#ifdef __VIDEOCORE__
#include "vc/intrinsics.h"
#undef asm
#define asm(x) _ASM(x)

#undef min
#define min(x,y) _min(x,y)

#undef max
#define max(x,y) _max(x,y)

#ifndef abs
#define abs(x) _abs(x)
#endif
#else
#define _vasm asm
#define _bkpt() do {asm(" bkpt");}while(0)
#define _di() do{asm(" di");}while(0)
#define _ei() do{asm(" ei");}while(0)
#define _nop() do{asm(" nop");}while(0)
#define _sleep() do{asm(" sleep");}while(0)

#undef min
#define min(x,y) ((x)<(y) ? (x):(y))

#undef max
#define max(x,y) ((x)>(y) ? (x):(y))

#ifndef abs
#define abs(x) ((x)>=0 ? (x):-(x))
#endif
#endif

#endif
