/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vector_list.c).
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

#include <boolean.h>
#include <stdlib.h>
#include <stddef.h>

/* default type is void*, override by defining VECTOR_LIST_TYPE before inclusion */
#ifndef VECTOR_LIST_TYPE
#define VECTOR_LIST_TYPE void*
#define VECTOR_LIST_TYPE_DEFINED
#endif

/* default name is void, override by defining VECTOR_LIST_NAME before inclusion */
#ifndef VECTOR_LIST_NAME
#define VECTOR_LIST_NAME void
#define VECTOR_LIST_NAME_DEFINED
#endif

#define CAT_I(a,b) a##b
#define CAT(a,b) CAT_I(a, b)
#define MAKE_TYPE_NAME() CAT(VECTOR_LIST_NAME, _vector_list)
#define TYPE_NAME() MAKE_TYPE_NAME()

struct TYPE_NAME()
{
   /* VECTOR_LIST_TYPE for pointers will expand to a pointer-to-pointer */
   VECTOR_LIST_TYPE *data;
   unsigned size;
   unsigned count;
};

static struct TYPE_NAME()* CAT(TYPE_NAME(), _new(void))
{
   struct TYPE_NAME() *list = (struct TYPE_NAME()*)calloc(1, sizeof(*list));

   list->size = 8;
   list->data = (VECTOR_LIST_TYPE*)calloc(list->size, sizeof(*list->data));

   return list;
}

static bool CAT(TYPE_NAME(), _append(struct TYPE_NAME() *list, VECTOR_LIST_TYPE elem))
{
   if (list->size == list->count)
   {
      list->size *= 2;
      list->data = (VECTOR_LIST_TYPE*)realloc(list->data, list->size * sizeof(*list->data));

      if (!list->data)
         return false;
   }

   list->data[list->count] = elem;
   list->count++;

   return true;
}

static void CAT(TYPE_NAME(), _free(struct TYPE_NAME() *list))
{
   if (list)
   {
      if (list->data)
         free(list->data);
      free(list);
   }
}

#ifdef VECTOR_LIST_TYPE_DEFINED
#undef VECTOR_LIST_TYPE
#endif

#ifdef VECTOR_LIST_NAME_DEFINED
#undef VECTOR_LIST_NAME
#endif
