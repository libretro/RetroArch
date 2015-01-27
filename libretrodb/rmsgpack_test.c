#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

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
	printf("b'%s'", s);
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
   int fd;
   /*
      int fd = open("test.msgpack", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
      int rv = 0;
      if (fd == -1)
      {
      printf("Could not open file: %s", strerror(errno));
      return errno;
      }
      rmsgpack_write_map_header(fd, 2);
      rmsgpack_write_string(fd, "compact", strlen("compact"));
      rmsgpack_write_bool(fd, 1);
      rmsgpack_write_string(fd, "schema", strlen("schema"));
      rmsgpack_write_array_header(fd, 10);
      rmsgpack_write_string(fd, "schema", strlen("schema"));
      rmsgpack_write_uint(fd, 1<<17);
      rmsgpack_write_int(fd, (1<<17) + 1);
      rmsgpack_write_int(fd, 4);
      rmsgpack_write_int(fd, -3);
      rmsgpack_write_int(fd, -22);
      rmsgpack_write_int(fd, -35);
      rmsgpack_write_int(fd, -421421412);
      rmsgpack_write_int(fd, 4214);
      rmsgpack_write_int(fd, -4214);
      rmsgpack_write_uint(fd, 1<<17);
      close(fd);
      */

   struct stub_state state;

   state.i = 0;
   state.stack[0] = 0;
   fd = open("test.msgpack", O_RDONLY);
   rmsgpack_read(fd, &stub_callbacks, &state);
   printf("\n");
   close(fd);

   return 0;
}
