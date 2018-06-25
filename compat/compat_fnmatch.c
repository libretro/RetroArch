/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_fnmatch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if __TEST_FNMATCH__
#include <assert.h>
#endif
#include <stddef.h>

#include <compat/fnmatch.h>

/* Implemnentation of fnmatch(3) so it can be
 * distributed to non *nix platforms.
 *
 * No flags are implemented ATM.
 * We don't use them. Add flags as needed. */

int rl_fnmatch(const char *pattern, const char *string, int flags)
{
   int rv;
   const char *c = NULL;
   int charmatch = 0;

   for (c = pattern; *c != '\0'; c++)
   {
      /* String ended before pattern */
      if ((*c != '*') && (*string == '\0'))
         return FNM_NOMATCH;

      switch (*c)
      {
         /* Match any number of unknown chars */
         case '*':
            /* Find next node in the pattern
             * ignoring multiple asterixes
             */
            do {
               c++;
               if (*c == '\0')
                  return 0;
            } while (*c == '*');

            /* Match the remaining pattern
             * ignoring more and more characters. */
            do {
               /* We reached the end of the string without a
                * match. There is a way to optimize this by
                * calculating the minimum chars needed to
                * match the remaining pattern but I don't
                * think it is worth the work ATM.
                */
               if (*string == '\0')
                  return FNM_NOMATCH;

               rv = rl_fnmatch(c, string, flags);
               string++;
            } while (rv != 0);

            return 0;
            /* Match char from list */
         case '[':
            charmatch = 0;
            for (c++; *c != ']'; c++)
            {
               /* Bad format */
               if (*c == '\0')
                  return FNM_NOMATCH;

               /* Match already found */
               if (charmatch)
                  continue;

               if (*c == *string)
                  charmatch = 1;
            }

            /* No match in list */
            if (!charmatch)
               return FNM_NOMATCH;

            string++;
            break;
            /* Has any character */
         case '?':
            string++;
            break;
            /* Match following character verbatim */
         case '\\':
            c++;
            /* Dangling escape at end of pattern.
             * FIXME: Was c == '\0' (makes no sense).
             * Not sure if c == NULL or *c == '\0'
             * is intended. Assuming *c due to c++ right before. */
            if (*c == '\0')
               return FNM_NOMATCH;
         default:
            if (*c != *string)
               return FNM_NOMATCH;
            string++;
      }
   }

   /* End of string and end of pattend */
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
