/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Ficoos
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

#if __TEST_FNMATCH__
#include <assert.h>
#endif

#include "rarch_fnmatch.h"

// Implemnentation of fnmatch(3) so it can be distributed to non *nix platforms
// No flags are implemented ATM. We don't use them. Add flags as needed.

int rl_fnmatch(const char *pattern, const char *string, int flags)
{
   const char *c;
   int charmatch = 0;
   int rv;
   for (c = pattern; *c != '\0'; c++)
   {
      // String ended before pattern
      if ((*c != '*') && (*string == '\0'))
         return FNM_NOMATCH;

      switch (*c)
      {
         // Match any number of unknown chars
         case '*':
            // Find next node in the pattern ignoring multiple
            // asterixes
            do {
               c++;
               if (*c == '\0')
                  return 0;
            } while (*c == '*');

            // Match the remaining pattern ingnoring more and more
            // chars.
            do {
               // We reached the end of the string without a
               // match. There is a way to optimize this by
               // calculating the minimum chars needed to
               // match the remaining pattern but I don't
               // think it is worth the work ATM.
               if (*string == '\0')
                  return FNM_NOMATCH;

               rv = rl_fnmatch(c, string, flags);
               string++;
            } while (rv != 0);

            return 0;
            // Match char from list
         case '[':
            charmatch = 0;
            for (c++; *c != ']'; c++)
            {
               // Bad formath
               if (*c == '\0')
                  return FNM_NOMATCH;

               // Match already found
               if (charmatch)
                  continue;

               if (*c == *string)
                  charmatch = 1;
            }

            // No match in list
            if (!charmatch)
               return FNM_NOMATCH;

            string++;
            break;
            // Has any char
         case '?':
            string++;
            break;
            // Match following char verbatim
         case '\\':
            c++;
            // Dangling escape at end of pattern
            if (*c == '\0') // FIXME: Was c == '\0' (makes no sense). Not sure if c == NULL or *c == '\0' is intended. Assuming *c due to c++ right before.
               return FNM_NOMATCH;
         default:
            if (*c != *string)
               return FNM_NOMATCH;
            string++;
      }
   }

   // End of string and end of pattend
   if (*string == '\0')
      return 0;
   return FNM_NOMATCH;
}

#if __TEST_FNMATCH__
int main(void)
{
   assert(rl_fnmatch("TEST", "TEST", 0) == 0);
   assert(rl_fnmatch("TE?T", "TEST", 0) == 0);
   assert(rl_fnmatch("TE[Ssa]T", "TEST", 0) == 0);
   assert(rl_fnmatch("TE[Ssda]T", "TEsT", 0) == 0);
   assert(rl_fnmatch("TE[Ssda]T", "TEdT", 0) == 0);
   assert(rl_fnmatch("TE[Ssda]T", "TEaT", 0) == 0);
   assert(rl_fnmatch("TEST*", "TEST", 0) == 0);
   assert(rl_fnmatch("TEST**", "TEST", 0) == 0);
   assert(rl_fnmatch("TE*ST*", "TEST", 0) == 0);
   assert(rl_fnmatch("TE**ST*", "TEST", 0) == 0);
   assert(rl_fnmatch("TE**ST*", "TExST", 0) == 0);
   assert(rl_fnmatch("TE**ST", "TEST", 0) == 0);
   assert(rl_fnmatch("TE**ST", "TExST", 0) == 0);
   assert(rl_fnmatch("TE\\**ST", "TE*xST", 0) == 0);
   assert(rl_fnmatch("*.*", "test.jpg", 0) == 0);
   assert(rl_fnmatch("*.jpg", "test.jpg", 0) == 0);
   assert(rl_fnmatch("*.[Jj][Pp][Gg]", "test.jPg", 0) == 0);
   assert(rl_fnmatch("*.[Jj]*[Gg]", "test.jPg", 0) == 0);
   assert(rl_fnmatch("TEST?", "TEST", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TES[asd", "TEST", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TEST\\", "TEST", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TEST*S", "TEST", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TE**ST", "TExT", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TE\\*T", "TExT", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TES?", "TES", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TE", "TEST", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("TEST!", "TEST", 0) == FNM_NOMATCH);
   assert(rl_fnmatch("DSAD", "TEST", 0) == FNM_NOMATCH);
}
#endif
