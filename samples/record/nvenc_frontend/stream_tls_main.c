/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <net/net_socket_ssl.h>
int rarch_main(int argc, char *argv[], void *data);
/* Test entry only: trust the local receiver's CA without changing the host. */
int main(int argc, char *argv[])
{
   const char *path = getenv("RTMPS_CA");
   char *pem;
   FILE *file;
   long size;
   int result;
   if (!path || !(file = fopen(path, "rb")))
      return 2;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 || size > 65536
         || fseek(file, 0, SEEK_SET))
   {
      fclose(file);
      return 2;
   }
   pem = (char*)malloc((size_t)size);
   if (!pem)
   {
      fclose(file);
      return 2;
   }
   if (fread(pem, 1, (size_t)size, file) != (size_t)size)
   {
      fclose(file);
      free(pem);
      return 2;
   }
   fclose(file);
   ssl_socket_retro_set_trust_pem(pem, (size_t)size);
   result = rarch_main(argc, argv, NULL);
   ssl_socket_retro_set_trust_pem(NULL, 0);
   free(pem);
   return result;
}
