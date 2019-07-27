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
#include <math.h>

#include <string/stdstring.h>

#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"

#include "d3d_common.h"

#define D3D_TEXTURE_FILTER_LINEAR 2
#define D3D_TEXTURE_FILTER_POINT  1

void *d3d_matrix_transpose(void *_pout, const void *_pm)
{
   unsigned i,j;
   struct d3d_matrix       *pout = (struct d3d_matrix*)_pout;
   const struct d3d_matrix *pm   = (struct d3d_matrix*)_pm;

   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < 4; j++)
         pout->m[i][j] = pm->m[j][i];
   }
   return pout;
}

void *d3d_matrix_identity(void *_pout)
{
   struct d3d_matrix *pout = (struct d3d_matrix*)_pout;
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

void *d3d_matrix_ortho_off_center_lh(void *_pout,
      float l, float r, float b, float t, float zn, float zf)
{
   struct d3d_matrix *pout = (struct d3d_matrix*)_pout;

   d3d_matrix_identity(pout);

   pout->m[0][0] = 2.0f / (r - l);
   pout->m[1][1] = 2.0f / (t - b);
   pout->m[2][2] = 1.0f / (zf -zn);
   pout->m[3][0] = -1.0f -2.0f *l / (r - l);
   pout->m[3][1] = 1.0f + 2.0f * t / (b - t);
   pout->m[3][2] = zn / (zn -zf);
   return pout;
}

void *d3d_matrix_multiply(void *_pout,
      const void *_pm1, const void *_pm2)
{
   unsigned i,j;
   struct d3d_matrix      *pout = (struct d3d_matrix*)_pout;
   const struct d3d_matrix *pm1 = (const struct d3d_matrix*)_pm1;
   const struct d3d_matrix *pm2 = (const struct d3d_matrix*)_pm2;

   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < 4; j++)
         pout->m[i][j] = pm1->m[i][0] *
            pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] +
                           pm1->m[i][2] * pm2->m[2][j] +
                           pm1->m[i][3] * pm2->m[3][j];
   }
   return pout;
}

void *d3d_matrix_rotation_z(void *_pout, float angle)
{
   struct d3d_matrix *pout = (struct d3d_matrix*)_pout;

   d3d_matrix_identity(pout);
   pout->m[0][0] = cos(angle);
   pout->m[1][1] = cos(angle);
   pout->m[0][1] = sin(angle);
   pout->m[1][0] = -sin(angle);
   return pout;
}

int32_t d3d_translate_filter(unsigned type)
{
   switch (type)
   {
      case RARCH_FILTER_UNSPEC:
         {
            settings_t *settings = config_get_ptr();
            if (!settings->bools.video_smooth)
               break;
         }
         /* fall-through */
      case RARCH_FILTER_LINEAR:
         return (int32_t)D3D_TEXTURE_FILTER_LINEAR;
      case RARCH_FILTER_NEAREST:
         break;
   }

   return (int32_t)D3D_TEXTURE_FILTER_POINT;
}

void d3d_input_driver(const char* input_name, const char* joypad_name,
      input_driver_t** input, void** input_data)
{
#if defined(__WINRT__)
   /* Plain xinput is supported on UWP, but it
    * supports joypad only (uwp driver was added later) */
   if (string_is_equal(input_name, "xinput"))
   {
      void *xinput = input_xinput.init(joypad_name);
      *input = xinput ? (input_driver_t*)&input_xinput : NULL;
      *input_data = xinput;
   }
   else
   {
      void *uwp = input_uwp.init(joypad_name);
      *input = uwp ? (input_driver_t*)&input_uwp : NULL;
      *input_data = uwp;
   }
#elif defined(_XBOX)
   void *xinput = input_xinput.init(joypad_name);
   *input = xinput ? (input_driver_t*)&input_xinput : NULL;
   *input_data = xinput;
#else
#if _WIN32_WINNT >= 0x0501
   /* winraw only available since XP */
   if (string_is_equal(input_name, "raw"))
   {
      *input_data = input_winraw.init(joypad_name);
      if (*input_data)
      {
         *input = &input_winraw;
         return;
      }
   }
#endif

#ifdef HAVE_DINPUT
   *input_data = input_dinput.init(joypad_name);
   *input = *input_data ? &input_dinput : NULL;
#endif
#endif
}
