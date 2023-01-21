/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2018 - Alfredo Monclus
 *  Copyright (C) 2019-2020 - Víctor González Fraile
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
 *
 * CHARACTER/LINES HARD LIMIT (MEASURED BY THE LOWEST COMMON DENOMINATOR, THE RGUI INTERFACE):
 * 48 CHARACTERS PER LINE
 * 20 LINES PER SCREEN
 *
 * LÍMITES FIJOS DE CARACTERES/LÍNEAS (MEDIDOS SEGÚN EL DENOMINADOR COMÚN MÁS BAJO, LA INTERFAZ RGUI):
 * 48 CARACTERES POR LÍNEA
 * 20 LÍNEAS POR PANTALLA
 */

#include <stdint.h>
#include <stddef.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

const char *msg_hash_to_str_es(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "msg_hash_es.h"
      default:
         break;
   }

   return "null";
}
