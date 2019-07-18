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
#include <boolean.h>
#include <string.h>

/*
 * Does not work with nested blocks.
 */
void label_sanitize(char *label, size_t size, char *lchars, char *rchars, int dcount)
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
         for (int dindex = 0; dindex < dcount; dindex++)
         {
            if (label[lindex] == lchars[dindex])
               copy = false;
         }

         if (copy)
            new_label[rindex++] = label[lindex];
      }
      else
      {
         // check for the end of the range
         for (int dindex = 0; dindex < dcount; dindex++)
         {
            if (label[lindex] == rchars[dindex])
               copy = true;
         }
      }
   }

   new_label[rindex] = label[lindex];

   strcpy(label, new_label);
}

void label_default_display(char *label, size_t size)
{
   return;
}

void label_remove_parens(char *label, size_t size)
{
   label_sanitize(label, size, "(", ")", 1);
}

void label_remove_brackets(char *label, size_t size)
{
   label_sanitize(label, size, "[", "]", 1);
}

void label_remove_parens_and_brackets(char *label, size_t size)
{
   label_sanitize(label, size, "([", ")]", 2);
}

void label_keep_region(char *label, size_t size)
{
   bool copy = true;
   int rindex = 0;
   int lindex = 0;
   char new_label[size];

   for (; lindex < size && label[lindex] != '\0'; lindex++)
   {
      if (copy)
      {
         if (label[lindex] == '(' || label[lindex] == '[')
         {
            if (!(label[lindex + 1] == 'E'
                  && label[lindex + 2] == 'u'
                  && label[lindex + 3] == 'r'
                  && label[lindex + 4] == 'o'
                  && label[lindex + 5] == 'p'
                  && label[lindex + 6] == 'e')
               && !(label[lindex + 1] == 'J'
                  && label[lindex + 2] == 'a'
                  && label[lindex + 3] == 'p'
                  && label[lindex + 4] == 'a'
                  && label[lindex + 5] == 'n')
               && !(label[lindex + 1] == 'U'
                  && label[lindex + 2] == 'S'
                  && label[lindex + 3] == 'A'))
               copy = false;
         }

         if (copy)
            new_label[rindex++] = label[lindex];
      }
      else if (label[lindex] == ')' || label[lindex] == ']')
         copy = true;
   }

   new_label[rindex] = label[lindex];

   strcpy(label, new_label);
}

void label_keep_disc(char *label, size_t size)
{
   bool copy = true;
   int rindex = 0;
   int lindex = 0;
   char new_label[size];

   for (; lindex < size && label[lindex] != '\0'; lindex++)
   {
      if (copy)
      {
         if (label[lindex] == '(' || label[lindex] == '[')
         {
            if (!(label[lindex + 1] == 'D'
                  && label[lindex + 2] == 'i'
                  && label[lindex + 3] == 's'
                  && label[lindex + 4] == 'c'))
               copy = false;
         }

         if (copy)
            new_label[rindex++] = label[lindex];
      }
      else if (label[lindex] == ')' || label[lindex] == ']')
         copy = true;
   }

   new_label[rindex] = label[lindex];

   strcpy(label, new_label);
}

void label_keep_region_and_disc(char *label, size_t size)
{
   bool copy = true;
   int rindex = 0;
   int lindex = 0;
   char new_label[size];

   for (; lindex < size && label[lindex] != '\0'; lindex++)
   {
      if (copy)
      {
         if (label[lindex] == '(' || label[lindex] == '[')
         {
            if (!(label[lindex + 1] == 'D'
                  && label[lindex + 2] == 'i'
                  && label[lindex + 3] == 's'
                  && label[lindex + 4] == 'c')
               && !(label[lindex + 1] == 'E'
                  && label[lindex + 2] == 'u'
                  && label[lindex + 3] == 'r'
                  && label[lindex + 4] == 'o'
                  && label[lindex + 5] == 'p'
                  && label[lindex + 6] == 'e')
               && !(label[lindex + 1] == 'J'
                  && label[lindex + 2] == 'a'
                  && label[lindex + 3] == 'p'
                  && label[lindex + 4] == 'a'
                  && label[lindex + 5] == 'n')
               && !(label[lindex + 1] == 'U'
                  && label[lindex + 2] == 'S'
                  && label[lindex + 3] == 'A'))
               copy = false;
         }

         if (copy)
            new_label[rindex++] = label[lindex];
      }
      else if (label[lindex] == ')' || label[lindex] == ']')
         copy = true;
   }

   new_label[rindex] = label[lindex];

   strcpy(label, new_label);
}
