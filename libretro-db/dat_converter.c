#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "rarchdb.h"
#include "db_parser.h"

#define MAX_TOKEN 256

static char *strndup_(const char *s, size_t n)
{
   char* buff = calloc(n, sizeof(char));

   if (!buff)
      return 0;

   strncpy(buff, s, n);
   return buff;
}

static struct rmsgpack_dom_value *get_map_value(
      const struct rmsgpack_dom_value *m, char* key)
{
   struct rmsgpack_dom_value v;

   v.type = RDT_STRING;
   v.string.len = strlen(key);
   v.string.buff = key;
   return rmsgpack_dom_value_map_value(m, &v);
}

static int load_string(int fd, struct rmsgpack_dom_value *out)
{
   char tok[MAX_TOKEN];
   ssize_t tok_size;

   if ((tok_size = get_token(fd, tok, MAX_TOKEN)) < 0)
      return tok_size;

   out->type = RDT_STRING;
   out->string.len = tok_size;
   out->string.buff = strndup_(tok, tok_size);
   return 0;
}

static int load_uint(int fd, struct rmsgpack_dom_value *out)
{
   char tok[MAX_TOKEN], *c;
   ssize_t tok_size;
   uint64_t value = 0;

   if ((tok_size = get_token(fd, tok, MAX_TOKEN)) < 0)
      return tok_size;

   for (c = tok; c < tok + tok_size; c++)
   {
      value *= 10;
      value += *c - '0';
   }

   out->type = RDT_UINT;
   out->uint_ = value;
   return 0;
}

static int load_bin(int fd, struct rmsgpack_dom_value *out)
{
   char tok[MAX_TOKEN];
   ssize_t tok_size;
   uint8_t h;
   uint8_t l;
   int i;

   if ((tok_size = get_token(fd, tok, MAX_TOKEN)) < 0)
      return tok_size;

   out->type = RDT_BINARY;
   out->binary.len = tok_size / 2;

   for (i = 0; i < tok_size; i += 2)
   {
      if (tok[i] <= '9')
         h = tok[i] - '0';
      else
         h = (tok[i] - 'A') + 10;
      if (tok[i+1] <= '9')
         l = tok[i+1] - '0';
      else
         l = (tok[i+1] - 'A') + 10;
      tok[i/2] = h * 16 + l;
   }

   out->binary.buff = malloc(out->binary.len);
   memcpy(out->binary.buff, tok, out->binary.len);
   return 0;
}

static int dat_value_provider(void *ctx, struct rmsgpack_dom_value *out)
{
   int rv, i;
   static const int field_count = 22;
   int fd = *((int*)ctx);
   char* key;

   out->type = RDT_MAP;
   out->map.len = field_count;
   out->map.items = calloc(field_count, sizeof(struct rmsgpack_dom_pair));

   if (find_token(fd, "game") < 0)
      return 1;

   for (i = 0; i < field_count; i++)
   {
      if ((rv = load_string(fd, &out->map.items[i].key)) < 0)
         goto failed;

      key = out->map.items[i].key.string.buff;

      if (strncmp(key, "name", sizeof("name")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "description", sizeof("description")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "users", sizeof("users")) == 0)
      {
         if ((rv = load_uint(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "releasemonth", sizeof("releasemonth")) == 0)
      {
         if ((rv = load_uint(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "releaseyear", sizeof("releaseyear")) == 0)
      {
         if ((rv = load_uint(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "rumble", sizeof("rumble")) == 0)
      {
         if ((rv = load_uint(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "analog", sizeof("analog")) == 0)
      {
         if ((rv = load_uint(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "serial", sizeof("serial")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "esrb_rating", sizeof("esrb_rating")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "elspa_rating", sizeof("elspa_rating")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "pegi_rating", sizeof("pegi_rating")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "cero_rating", sizeof("cero_rating")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "developer", sizeof("developer")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "publisher", sizeof("publisher")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "origin", sizeof("origin")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "rom", sizeof("rom")) == 0)
      {
         if (find_token(fd, "name") < 0)
            goto failed;

         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "size", sizeof("size")) == 0)
      {
         if ((rv = load_uint(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "sha1", sizeof("sha1")) == 0)
      {
         if ((rv = load_bin(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "crc", sizeof("crc")) == 0)
      {
         if ((rv = load_bin(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "md5", sizeof("md5")) == 0)
      {
         if ((rv = load_bin(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, "serial", sizeof("serial")) == 0)
      {
         if ((rv = load_string(fd, &out->map.items[i].value)) < 0)
            goto failed;
      }
      else if (strncmp(key, ")", sizeof(")")) == 0)
      {
         rmsgpack_dom_value_free(&out->map.items[i].key);
         out->map.len = i;
         printf("Couldn't find all fields for item\n");
         break;
      }
      else
      {
         rmsgpack_dom_value_free(&out->map.items[i].key);
         i--;
      }
   }
   printf("Inserting '%s' (%02X%02X%02X%02X)...\n",
         get_map_value(out, "name")->string.buff,
         (unsigned char)get_map_value(out, "crc")->binary.buff[0],
         (unsigned char)get_map_value(out, "crc")->binary.buff[1],
         (unsigned char)get_map_value(out, "crc")->binary.buff[2],
         (unsigned char)get_map_value(out, "crc")->binary.buff[3]
         );
   return 0;

failed:
   rmsgpack_dom_value_free(out);
   out->type = RDT_NULL;
   return rv;
}

int main(int argc, char **argv)
{
   int rv = 0;
   int src = -1;
   int dst = -1;

   if (argc != 3)
      printf("Usage: %s <dat file> <output file>\n", argv[0]);

   src = open(argv[1], O_RDONLY);

   if (src == -1)
   {
      printf("Could not open source file '%s': %s\n", argv[1], strerror(errno));
      rv = errno;
      goto clean;
   }

   dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

   if (dst == -1)
   {
      printf("Could not open destination file '%s': %s\n", argv[1], strerror(errno));
      rv = errno;
      goto clean;
   }

   rv = rarchdb_create(dst, &dat_value_provider, &src);

clean:
   if (src != -1)
      close(src);
   if (dst != -1)
      close(dst);
   return rv;
}
