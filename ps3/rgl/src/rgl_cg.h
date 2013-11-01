/*  RetroArch - A frontend for libretro.
 *  RGL - An OpenGL subset wrapper library.
 *  Copyright (C) 2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RGL_CG_H
#define _RGL_CG_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <algorithm>
#include <iostream>

#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include <Cg/CgCommon.h>
#include <Cg/cgBinary.h>
#include <Cg/CgInternal.h>
#include <Cg/CgProgramGroup.h>
#include <Cg/cgc.h>

#include "../include/export/RGL/rgl.h"
#include "../include/RGL/private.h"
#include "../include/RGL/Types.h"
#include "../include/RGL/Utils.h"
#include "libelf/readelf.h"

#include "cg/cgbtypes.h"
#include "cg/cgnv2rt.h"
#include "cg/cgnv2elfversion.h"

#include "cg/cgbio.hpp"
#include "cg/cgbiimpl.hpp"
#include "cg/nvbiimpl.hpp"
#include "cg/cgbutils.hpp"
#include "cg/cgbtypes.h"

typedef struct
{
   const char* elfFile;
   size_t elfFileSize;

   const char *symtab;
   size_t symbolSize;
   size_t symbolCount;
   const char *symbolstrtab;

   const char* shadertab;
   size_t shadertabSize;
   const char* strtab;
   size_t strtabSize;
   const char* consttab;
   size_t consttabSize;
} CGELFBinary;

typedef struct
{
   const char *texttab;
   size_t texttabSize;
   const char *paramtab;
   size_t paramtabSize;
   int index;
} CGELFProgram;

extern int rglpCopyProgram (void *src_data, void *dst_data);
extern int rglpGenerateFragmentProgram (void *data,
   const CgProgramHeader *programHeader, const void *ucode,
   const CgParameterTableHeader *parameterHeader,
   const char *stringTable, const float *defaultValues );

extern int rglpGenerateVertexProgram (void *data,
   const CgProgramHeader *programHeader, const void *ucode,
   const CgParameterTableHeader *parameterHeader, const char *stringTable,
   const float *defaultValues );

extern CGprogram rglpCgUpdateProgramAtIndex( CGprogramGroup group, int index,
   int refcount );

extern void rglpProgramErase (void *data);

extern bool cgOpenElf( const void *ptr, size_t size, CGELFBinary *elfBinary );
extern bool cgGetElfProgramByIndex( CGELFBinary *elfBinary, int index, CGELFProgram *elfProgram );

extern CGprogram rglCgCreateProgram( CGcontext ctx, CGprofile profile, const CgProgramHeader *programHeader, const void *ucode, const CgParameterTableHeader *parameterHeader, const char *stringTable, const float *defaultValues );

#endif
