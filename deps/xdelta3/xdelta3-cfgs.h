/* xdelta3 - delta compression tools and library
   Copyright 2016 Joshua MacDonald

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/******************************************************************
 SOFT string matcher
 ******************************************************************/

#if XD3_BUILD_SOFT

#define TEMPLATE      soft
#define LLOOK         stream->smatcher.large_look
#define LSTEP         stream->smatcher.large_step
#define SLOOK         stream->smatcher.small_look
#define SCHAIN        stream->smatcher.small_chain
#define SLCHAIN       stream->smatcher.small_lchain
#define MAXLAZY       stream->smatcher.max_lazy
#define LONGENOUGH    stream->smatcher.long_enough

#define SOFTCFG 1
#include "xdelta3.c"
#undef  SOFTCFG

#undef  TEMPLATE
#undef  LLOOK
#undef  SLOOK
#undef  LSTEP
#undef  SCHAIN
#undef  SLCHAIN
#undef  MAXLAZY
#undef  LONGENOUGH
#endif

#define SOFTCFG 0

/************************************************************
 FASTEST string matcher
 **********************************************************/
#if XD3_BUILD_FASTEST
#define TEMPLATE      fastest
#define LLOOK         9
#define LSTEP         26
#define SLOOK         4U
#define SCHAIN        1
#define SLCHAIN       1
#define MAXLAZY       6
#define LONGENOUGH    6

#include "xdelta3.c"

#undef  TEMPLATE
#undef  LLOOK
#undef  SLOOK
#undef  LSTEP
#undef  SCHAIN
#undef  SLCHAIN
#undef  MAXLAZY
#undef  LONGENOUGH
#endif

/************************************************************
 FASTER string matcher
 **********************************************************/
#if XD3_BUILD_FASTER
#define TEMPLATE      faster
#define LLOOK         9
#define LSTEP         15
#define SLOOK         4U
#define SCHAIN        1
#define SLCHAIN       1
#define MAXLAZY       18
#define LONGENOUGH    18

#include "xdelta3.c"

#undef  TEMPLATE
#undef  LLOOK
#undef  SLOOK
#undef  LSTEP
#undef  SCHAIN
#undef  SLCHAIN
#undef  MAXLAZY
#undef  LONGENOUGH
#endif

/******************************************************
 FAST string matcher
 ********************************************************/
#if XD3_BUILD_FAST
#define TEMPLATE      fast
#define LLOOK         9
#define LSTEP         8
#define SLOOK         4U
#define SCHAIN        4
#define SLCHAIN       1
#define MAXLAZY       18
#define LONGENOUGH    35

#include "xdelta3.c"

#undef  TEMPLATE
#undef  LLOOK
#undef  SLOOK
#undef  LSTEP
#undef  SCHAIN
#undef  SLCHAIN
#undef  MAXLAZY
#undef  LONGENOUGH
#endif

/**************************************************
 SLOW string matcher
 **************************************************************/
#if XD3_BUILD_SLOW
#define TEMPLATE      slow
#define LLOOK         9
#define LSTEP         2
#define SLOOK         4U
#define SCHAIN        44
#define SLCHAIN       13
#define MAXLAZY       90
#define LONGENOUGH    70

#include "xdelta3.c"

#undef  TEMPLATE
#undef  LLOOK
#undef  SLOOK
#undef  LSTEP
#undef  SCHAIN
#undef  SLCHAIN
#undef  MAXLAZY
#undef  LONGENOUGH
#endif

/********************************************************
 DEFAULT string matcher
 ************************************************************/
#if XD3_BUILD_DEFAULT
#define TEMPLATE      default
#define LLOOK         9
#define LSTEP         3
#define SLOOK         4U
#define SCHAIN        8
#define SLCHAIN       2
#define MAXLAZY       36
#define LONGENOUGH    70

#include "xdelta3.c"

#undef  TEMPLATE
#undef  LLOOK
#undef  SLOOK
#undef  LSTEP
#undef  SCHAIN
#undef  SLCHAIN
#undef  MAXLAZY
#undef  LONGENOUGH
#endif
