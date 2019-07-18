/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_path.h).
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

#include <playlists/label_sanitization.h>
#include <string.h>

/*
 * Does not work with nested blocks.
 */
void label_sanitize(char *label, size_t size, bool (*left)(char*), bool (*right)(char*))
{
   bool copy = true;
   int rindex = 0;
   int lindex = 0;
   char new_label[size];

   for (; lindex < size && label[lindex] != '\0'; lindex++)
   {
      if (copy)
      {
         // check for the start of the range
         if ((*left)(&label[lindex]))
            copy = false;

         if (copy)
            new_label[rindex++] = label[lindex];
      }
      else if ((*right)(&label[lindex]))
         copy = true;
   }

   new_label[rindex] = label[lindex];

   strcpy(label, new_label);
}

bool left_parens(char *left)
{
   return left[0] == '(';
}

bool right_parens(char *right)
{
   return right[0] == ')';
}

bool left_brackets(char *left)
{
   return left[0] == '[';
}

bool right_brackets(char *right)
{
   return right[0] == ']';
}

bool left_parens_or_brackets(char *left)
{
   return left[0] == '(' || left[0] == '[';
}

bool right_parens_or_brackets(char *right)
{
   return right[0] == ']' || right[0] == ']';
}

bool left_parens_or_brackets_excluding_region(char *left)
{
   if (left_parens_or_brackets(left))
   {
      if ((left[1] == 'A'
            && left[2] == 'u'
            && left[3] == 's'
            && left[4] == 'r'
            && left[5] == 'a'
            && left[6] == 'l'
            && left[7] == 'i'
            && left[8] == 'a')
         || (left[1] == 'E'
            && left[2] == 'u'
            && left[3] == 'r'
            && left[4] == 'o'
            && left[5] == 'p'
            && left[6] == 'e')
         || (left[1] == 'J'
            && left[2] == 'a'
            && left[3] == 'p'
            && left[4] == 'a'
            && left[5] == 'n')
         || (left[1] == 'U'
            && left[2] == 'S'
            && left[3] == 'A'))
         return false;
      else
         return true;
   }
   else
      return false;
}

bool left_parens_or_brackets_excluding_disc(char *left)
{
   if (left_parens_or_brackets(left))
   {
      if (left[1] == 'D'
         && left[2] == 'i'
         && left[3] == 's'
         && left[4] == 'c')
         return false;
      else
         return true;
   }
   else
      return false;
}

bool left_parens_or_brackets_excluding_region_or_disc(char *left)
{
   if (left_parens_or_brackets(left))
   {
      if ((left[1] == 'A'
            && left[2] == 'u'
            && left[3] == 's'
            && left[4] == 'r'
            && left[5] == 'a'
            && left[6] == 'l'
            && left[7] == 'i'
            && left[8] == 'a')
         || (left[1] == 'E'
            && left[2] == 'u'
            && left[3] == 'r'
            && left[4] == 'o'
            && left[5] == 'p'
            && left[6] == 'e')
         || (left[1] == 'J'
            && left[2] == 'a'
            && left[3] == 'p'
            && left[4] == 'a'
            && left[5] == 'n')
         || (left[1] == 'U'
            && left[2] == 'S'
            && left[3] == 'A')
         || (left[1] == 'D'
            && left[2] == 'i'
            && left[3] == 's'
            && left[4] == 'c'))
         return false;
      else
         return true;
   }
   else
      return false;
}

void label_default_display(char *label, size_t size)
{
   return;
}

void label_remove_parens(char *label, size_t size)
{
   label_sanitize(label, size, left_parens, right_parens);
}

void label_remove_brackets(char *label, size_t size)
{
   label_sanitize(label, size, left_brackets, right_brackets);
}

void label_remove_parens_and_brackets(char *label, size_t size)
{
   label_sanitize(label, size, left_parens_or_brackets, right_parens_or_brackets);
}

void label_keep_region(char *label, size_t size)
{
   label_sanitize(label, size, left_parens_or_brackets_excluding_region, right_parens_or_brackets);
}

void label_keep_disc(char *label, size_t size)
{
   label_sanitize(label, size, left_parens_or_brackets_excluding_disc, right_parens_or_brackets);
}

void label_keep_region_and_disc(char *label, size_t size)
{
   label_sanitize(label, size, left_parens_or_brackets_excluding_region_or_disc, right_parens_or_brackets);
}
