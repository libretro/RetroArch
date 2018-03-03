/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../../configuration.h"
#include "../../verbosity.h"

#if defined(HAVE_D3D9)
#include <d3d9.h>

#ifdef HAVE_D3DX
#ifdef _XBOX
#include <d3dx9core.h>
#include <d3dx9tex.h>
#else
#include "../include/d3d9/d3dx9tex.h"
#endif

#endif
#endif

#if defined(HAVE_D3D8)
#include <d3d8.h>

#ifdef HAVE_D3DX
#ifdef _XBOX
#include <d3dx8core.h>
#include <d3dx8tex.h>
#else
#include "../include/d3d8/d3dx8tex.h"
#endif
#endif

#endif

#include "d3d_common.h"

void *d3d_matrix_transpose(void *_pout, const void *_pm)
{
   unsigned i,j;
   D3DMATRIX     *pout = (D3DMATRIX*)_pout;
   CONST D3DMATRIX *pm = (D3DMATRIX*)_pm;

   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < 4; j++)
         pout->m[i][j] = pm->m[j][i];
   }
   return pout;
}


void *d3d_matrix_identity(void *_pout)
{
   D3DMATRIX *pout = (D3DMATRIX*)_pout;
   if ( !pout )
      return NULL;

   pout->m[0][1] = 0.0f;
   pout->m[0][2] = 0.0f;
   pout->m[0][3] = 0.0f;
   pout->m[1][0] = 0.0f;
   pout->m[1][2] = 0.0f;
   pout->m[1][3] = 0.0f;
   pout->m[2][0] = 0.0f;
   pout->m[2][1] = 0.0f;
   pout->m[2][3] = 0.0f;
   pout->m[3][0] = 0.0f;
   pout->m[3][1] = 0.0f;
   pout->m[3][2] = 0.0f;
   pout->m[0][0] = 1.0f;
   pout->m[1][1] = 1.0f;
   pout->m[2][2] = 1.0f;
   pout->m[3][3] = 1.0f;
   return pout;
}

void *d3d_matrix_ortho_off_center_lh(void *_pout, float l, float r, float b, float t, float zn, float zf)
{
   D3DMATRIX *pout = (D3DMATRIX*)_pout;

   d3d_matrix_identity(pout);

   pout->m[0][0] = 2.0f / (r - l);
   pout->m[1][1] = 2.0f / (t - b);
   pout->m[2][2] = 1.0f / (zf -zn);
   pout->m[3][0] = -1.0f -2.0f *l / (r - l);
   pout->m[3][1] = 1.0f + 2.0f * t / (b - t);
   pout->m[3][2] = zn / (zn -zf);
   return pout;
}

void *d3d_matrix_multiply(void *_pout, const void *_pm1, const void *_pm2)
{
   unsigned i,j;
   D3DMATRIX      *pout = (D3DMATRIX*)_pout;
   CONST D3DMATRIX *pm1 = (CONST D3DMATRIX*)_pm1;
   CONST D3DMATRIX *pm2 = (CONST D3DMATRIX*)_pm2;

   for (i=0; i<4; i++)
   {
      for (j=0; j<4; j++)
         pout->m[i][j] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + 
                         pm1->m[i][2] * pm2->m[2][j] + pm1->m[i][3] * pm2->m[3][j];
   }
   return pout;
}

void *d3d_matrix_rotation_z(void *_pout, float angle)
{
   D3DMATRIX *pout = (D3DMATRIX*)_pout;
   d3d_matrix_identity(pout);
   pout->m[0][0] = cos(angle);
   pout->m[1][1] = cos(angle);
   pout->m[0][1] = sin(angle);
   pout->m[1][0] = -sin(angle);
   return pout;
}
