/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmsgpack_test.c).
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

#include <streams/file_stream.h>

#include "rmsgpack.h"

struct stub_state
{
	int i;
	uint64_t stack[256];
};

static void stub_state_push_map(struct stub_state *s, uint32_t size)
{
	s->i++;
	s->stack[s->i] = 1;
	s->i++;
	s->stack[s->i] = size * 2;
	printf("{");
}

static void stub_state_push_array(struct stub_state *s, uint32_t size)
{
	s->i++;
	s->stack[s->i] = 2;
	s->i++;
	s->stack[s->i] = size;
	printf("[");
}

static void stub_state_pre_print(struct stub_state *s)
{
}

static void stub_state_post_print(struct stub_state *s)
{
   switch (s->stack[s->i - 1])
   {
      case 1:
         if (s->stack[s->i] % 2 == 0)
         {
            printf(": ");
            s->stack[s->i]--;
         }
         else if (s->stack[s->i] == 1)
         {
            printf("}");
            s->i -= 2;
            stub_state_post_print(s);
         }
         else
         {
            printf(", ");
            s->stack[s->i]--;
         }
         break;
      case 2:
         if (s->stack[s->i] == 1)
         {
            printf("]");
            s->i -= 2;
            stub_state_post_print(s);
         }
         else
         {
            printf(", ");
            s->stack[s->i]--;
         }
         break;
   }
}

static int stub_read_map_start(uint32_t size, void *data)
{
   stub_state_push_map(data, size);
   return 0;
}

static int stub_read_array_start(uint32_t size, void *data)
{
	stub_state_push_array(data, size);
	return 0;
}

static int stub_read_string(char *s, uint32_t len, void *data)
{
   stub_state_pre_print(data);
   printf("'%s'", s);
   stub_state_post_print(data);
   free(s);
   return 0;
}

static int stub_read_bin(
        void * s,
        uint32_t len,
        void * data
){
	stub_state_pre_print(data);
	printf("b'%s'", (char*)s);
	stub_state_post_print(data);
	free(s);
	return 0;
}

static int stub_read_uint(uint64_t value, void *data)
{
   stub_state_pre_print(data);
#ifdef _WIN32
   printf("%I64u", (unsigned long long)value);
#else
   printf("%llu", (unsigned long long)value);
#endif
   stub_state_post_print(data);
   return 0;
}

static int stub_read_nil(void * data)
{
   stub_state_pre_print(data);
   printf("nil");
   stub_state_post_print(data);
   return 0;
}

static int stub_read_int(int64_t value, void * data)
{
   stub_state_pre_print(data);
#ifdef _WIN32
   printf("%I64d", (signed long long)value);
#else
   printf("%lld", (signed long long)value);
#endif
   stub_state_post_print(data);
   return 0;
}

static int stub_read_bool(int value, void * data)
{
   stub_state_pre_print(data);
   if (value)
      printf("true");
   else
      printf("false");
   stub_state_post_print(data);
   return 0;
}

static struct rmsgpack_read_callbacks stub_callbacks = {
	stub_read_nil,
	stub_read_bool,
	stub_read_int,
	stub_read_uint,
	stub_read_string,
	stub_read_bin,
	stub_read_map_start,
	stub_read_array_start
};

int main(void)
{
   struct stub_state state;
   RFILE *fd = filestream_open("test.msgpack",
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   state.i        = 0;
   state.stack[0] = 0;

   rmsgpack_read(fd, &stub_callbacks, &state);

   printf("Test succeeded.\n");
   filestream_close(fd);

   return 0;
}
