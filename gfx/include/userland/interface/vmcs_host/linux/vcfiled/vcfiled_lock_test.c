/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/** Very simple test code for the vcfiled locking
  */
#include "vcfiled_check.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static void usage(const char *prog)
{
   fprintf(stderr, "usage: %s lock|check <lockfile>\n", prog);
   exit(1);
}

static void logmsg(int level, const char *fmt, ...)
{
   (void)level;

   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

int main(int argc, const char **argv)
{
   if (argc != 3)
   {
      usage(argv[0]);
   }
   const char *lockfile = argv[2];
   if (strcmp(argv[1], "lock") == 0)
   {
      int rc = vcfiled_lock(lockfile, logmsg);
      if (rc)
      {
         printf("failed to lock %s\n", lockfile);
         exit(1);
      }
      sleep(300);
   }
   else if (strcmp(argv[1], "check") == 0)
   {
      printf("%s\n",
             vcfiled_is_running(lockfile) ?
             "running" : "not running");
   }
   else
   {
      usage(argv[0]);
   }
   return 0;
}
