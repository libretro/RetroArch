/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <net/net_rtmp_reply.h>
#include <stdlib.h>
#include <string.h>

struct reply_container
{
   net_rtmp_span_t key;
   uint32_t remaining;
   unsigned root;
   bool named, value_pending;
};
struct net_rtmp_reply
{
   net_rtmp_reply_info_t info;
   struct reply_container stack[8];
   const uint8_t *data;
   size_t size, offset;
   unsigned depth, roots, objects, fields;
   bool failed, done;
};

static bool reply_take(net_rtmp_reply_t *r, size_t size, const uint8_t **data)
{
   if (size > r->size - r->offset)
      return false;
   *data = r->data + r->offset;
   r->offset += size;
   return true;
}

static uint32_t reply_uint(const uint8_t *p, unsigned size)
{
   uint32_t value = 0;
   unsigned i;
   for (i = 0; i < size; i++)
      value = (value << 8) | p[i];
   return value;
}

static bool reply_text(net_rtmp_reply_t *r, unsigned width, net_rtmp_span_t *text)
{
   const uint8_t *p;
   uint32_t size;
   if (!reply_take(r, width, &p))
      return false;
   size = reply_uint(p, width);
   if (!reply_take(r, size, &p))
      return false;
   text->data = p;
   text->size = size;
   return true;
}

static bool reply_name(net_rtmp_span_t key, const char *name, size_t size)
{
   return key.size == size && !memcmp(key.data, name, size);
}

static bool reply_integer(const uint8_t *p, uint32_t *value)
{
   uint64_t bits = ((uint64_t)reply_uint(p, 4) << 32) | reply_uint(p + 4, 4);
   uint64_t fraction = bits & (((uint64_t)1 << 52) - 1), mantissa;
   unsigned exponent = (unsigned)((bits >> 52) & 2047), shift;
   if (!exponent && !fraction)
   {
      *value = 0;
      return true;
   }
   if ((bits >> 63) || exponent < 1023 || exponent > 1054)
      return false;
   shift = 52 - (exponent - 1023);
   mantissa = ((uint64_t)1 << 52) | fraction;
   if (mantissa & (((uint64_t)1 << shift) - 1))
      return false;
   *value = (uint32_t)(mantissa >> shift);
   return true;
}

net_rtmp_reply_t *net_rtmp_reply_new(void)
{
   return (net_rtmp_reply_t*)calloc(1, sizeof(net_rtmp_reply_t));
}

bool net_rtmp_reply_begin(net_rtmp_reply_t *r, unsigned type, const void *data, size_t size)
{
   const uint8_t *p = (const uint8_t*)data;
   if (!r || r->data || !data || !size || size > 0xffffff
         || (type != 20 && type != 17) || (type == 17 && (size == 1 || p[0])))
      return false;
   r->data = p;
   r->size = size;
   r->offset = type == 17 ? 1 : 0;
   return true;
}

int net_rtmp_reply_step(net_rtmp_reply_t *r)
{
   struct reply_container *parent = NULL, *child;
   net_rtmp_span_t text, key;
   const uint8_t *p;
   unsigned root, marker, field = 0;
   uint32_t value = 0, count = 0;
   bool named;
   key.data = NULL;
   key.size = 0;
   if (!r || !r->data || r->failed)
      return NET_RTMP_REPLY_ERROR;
   if (r->done)
      return NET_RTMP_REPLY_DONE;
   if (r->depth)
   {
      parent = r->stack + r->depth - 1;
      if (!parent->named && !parent->remaining)
      {
         r->depth--;
         return NET_RTMP_REPLY_PROGRESS;
      }
      if (parent->named && !parent->value_pending)
      {
         if (r->size - r->offset >= 3 && !memcmp(r->data + r->offset, "\0\0\11", 3))
         {
            r->offset += 3;
            r->depth--;
            return NET_RTMP_REPLY_PROGRESS;
         }
         if (!reply_text(r, 2, &parent->key))
            goto failed;
         parent->value_pending = true;
         return NET_RTMP_REPLY_PROGRESS;
      }
   }
   else if (r->offset == r->size)
   {
      /* Legacy publishers may receive a name-only callback. */
      if (r->roots < 3 && !(r->roots == 1
               && reply_name(r->info.command, "onFCPublish", 11)))
         goto failed;
      r->done = true;
      return NET_RTMP_REPLY_DONE;
   }
   if (!reply_take(r, 1, &p))
      goto failed;
   marker = p[0];
   root = parent ? parent->root : r->roots++;
   if (parent)
   {
      if (parent->named)
      {
         key = parent->key;
         parent->value_pending = false;
      }
      else
         parent->remaining--;
   }
   if (!r->depth && ((root == 0 && marker != 2 && marker != 12)
            || (root == 1 && marker != 0)))
      goto failed;
   if (r->depth == 1 && root == 3 && parent->named)
   {
      if (reply_name(key, "code", 4))
         field = 1;
      else if (reply_name(key, "level", 5))
         field = 2;
      else if (reply_name(key, "description", 11))
         field = 4;
      if (field && ((r->fields & field) || (marker != 2 && marker != 12)))
         goto failed;
   }
   switch (marker)
   {
      case 0:
         if (!reply_take(r, 8, &p))
            goto failed;
         if (!r->depth && (root == 1 || root == 3))
         {
            if (!reply_integer(p, &value))
               goto failed;
            if (root == 1)
               r->info.transaction = value;
            else
            {
               r->info.stream = value;
               r->info.stream_valid = true;
            }
         }
         break;
      case 1:
         if (!reply_take(r, 1, &p))
            goto failed;
         break;
      case 2:
      case 12:
      case 15:
         if (!reply_text(r, marker == 2 ? 2 : 4, &text))
            goto failed;
         if (!r->depth && root == 0)
            r->info.command = text;
         if (field)
         {
            r->fields |= field;
            if (field == 1)
               r->info.code = text;
            else if (field == 2)
               r->info.level = text;
            else
               r->info.description = text;
         }
         break;
      case 5:
      case 6:
      case 13:
         break;
      case 7:
         if (!reply_take(r, 2, &p) || reply_uint(p, 2) >= r->objects)
            goto failed;
         break;
      case 11:
         if (!reply_take(r, 10, &p))
            goto failed;
         break;
      case 3:
      case 8:
      case 10:
      case 16:
         if (r->depth == 8)
            goto failed;
         named = marker != 10;
         if (marker == 8 || marker == 10)
         {
            if (!reply_take(r, 4, &p))
               goto failed;
            count = reply_uint(p, 4);
            if (!named && count > r->size - r->offset)
               goto failed;
         }
         if (marker == 16 && !reply_text(r, 2, &text))
            goto failed;
         r->objects++;
         child = r->stack + r->depth++;
         child->root = root;
         child->named = named;
         child->value_pending = false;
         child->remaining = count;
         break;
      default:
         goto failed;
   }
   return NET_RTMP_REPLY_PROGRESS;
failed:
   r->failed = true;
   return NET_RTMP_REPLY_ERROR;
}

bool net_rtmp_reply_get(net_rtmp_reply_t *r, net_rtmp_reply_info_t *info)
{
   if (!r || !r->done || r->failed || !info)
      return false;
   *info = r->info;
   return true;
}

void net_rtmp_reply_release(net_rtmp_reply_t *r)
{
   if (r)
      memset(r, 0, sizeof(*r));
}

void net_rtmp_reply_free(net_rtmp_reply_t *r)
{
   free(r);
}
