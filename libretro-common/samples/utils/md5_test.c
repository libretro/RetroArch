/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * (This is a heavily cut-down "BSD license".)
 *
 * This differs from Colin Plumb's older public domain implementation in that
 * no exactly 32-bit integer data type is required (any 32-bit or wider
 * unsigned integer data type will do), there's no compile-time endianness
 * configuration, and the function prototypes match OpenSSL's.  No code from
 * Colin Plumb's implementation has been reused; this comment merely compares
 * the properties of the two independent implementations.
 *
 * The primary goals of this implementation are portability and ease of use.
 * It is meant to be fast, but not as fast as possible.  Some known
 * optimizations are not included to reduce source code size and avoid
 * compile-time configuration.
 */
#include <stdio.h>
#include <string.h>

#include <lrc_hash.h>

int main (int argc, char *argv[])
{
   /* For each command line argument in turn:
    ** filename          -- prints message digest and name of file
    */
   int i;
   MD5_CTX ctx;
   FILE* file;
   size_t numread;
   char buffer[16384];
   unsigned char result[16];

   for (i = 1; i < argc; i++)
   {
      MD5_Init(&ctx);
      file = fopen(argv[i], "rb");

      if (file)
      {
         do
         {
            numread = fread((void*)buffer, 1, sizeof(buffer), file);

            if (numread)
            {
               MD5_Update(&ctx,(void*)buffer, numread);
            }
         }
         while (numread);

         fclose(file);
         MD5_Final(result, &ctx);
         printf("%02x%02x%02x%02x%02x%02x%02x%02x"
			          "%02x%02x%02x%02x%02x%02x%02x%02x %s\n",
			          result[ 0 ], result[ 1 ], result[ 2 ], result[ 3 ],
                result[ 4 ], result[ 5 ], result[ 6 ], result[ 7 ],
                result[ 8 ], result[ 9 ], result[ 10 ], result[ 11 ],
                result[ 12 ], result[ 13 ], result[ 14 ], result[ 15 ],
                argv[i]);
      }
   }

   return 0;
}
