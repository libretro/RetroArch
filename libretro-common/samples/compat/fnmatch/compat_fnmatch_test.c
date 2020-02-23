/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_fnmatch_test.c).
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

#include <assert.h>
#include <stddef.h>

#include <compat/fnmatch.h>

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
