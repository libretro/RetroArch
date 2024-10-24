/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_test.c).
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/rzip_stream.h>
#include <retro_miscellaneous.h>

#define FILE_TRANSFER_CHUNK_SIZE 4096

enum rzip_action_type
{
	RZIP_ACTION_QUERY = 0,
	RZIP_ACTION_COMPRESS,
	RZIP_ACTION_EXTRACT
};

static void rand_str(char *dst, size_t len)
{
   char charset[] = "0123456789"
         "abcdefghijklmnopqrstuvwxyz"
         "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

   while (len-- > 0)
   {
      size_t i = (double)rand() / RAND_MAX * (sizeof(charset) - 1);
      *dst++   = charset[i];
   }
   *dst = '\0';
}

int main(int argc, char *argv[])
{
   char in_file_path[PATH_MAX_LENGTH];
   char out_file_path[PATH_MAX_LENGTH];
   enum rzip_action_type action = RZIP_ACTION_QUERY;
   intfstream_t *in_file        = NULL;
   intfstream_t *out_file       = NULL;
   int64_t in_file_size         = 0;
   int64_t in_file_raw_size     = 0;
   int64_t out_file_size        = 0;
   int64_t file_size_diff       = 0;
   int64_t total_data_read      = 0;
   bool in_file_compressed      = false;
   bool valid_args              = false;
   bool in_place                = false;
   int ret                      = 1;

   in_file_path[0]  = '\0';
   out_file_path[0] = '\0';

   /* Parse arguments */
   if ((argc > 1) && !string_is_empty(argv[1]))
   {
      valid_args = true;

      if (string_is_equal(argv[1], "i"))
         action = RZIP_ACTION_QUERY;
      else if (string_is_equal(argv[1], "a"))
         action = RZIP_ACTION_COMPRESS;
      else if (string_is_equal(argv[1], "x"))
         action = RZIP_ACTION_EXTRACT;
      else
         valid_args = false;
   }

   /* Get input file path */
   if (valid_args && (argc > 2) && !string_is_empty(argv[2]))
   {
      strlcpy(in_file_path, argv[2], sizeof(in_file_path));
      path_resolve_realpath(in_file_path, sizeof(in_file_path), true);
      valid_args = valid_args && !string_is_empty(in_file_path);
   }
   else
      valid_args = false;

   /* Ensure arguments are valid */
   if (!valid_args)
   {
      fprintf(stderr, "Usage:\n");
      fprintf(stderr, "- Query file status: %s i <input file>\n", argv[0]);
      fprintf(stderr, "- Compress file:     %s a <input file> <output file (optional)>\n", argv[0]);
      fprintf(stderr, "- Extract file:      %s x <input file> <output file (optional)>\n", argv[0]);
      fprintf(stderr, "Omitting <output file> will overwrite <input file>\n");
      goto end;
   }

   /* Ensure that input file exists */
   if (!path_is_valid(in_file_path))
   {
      fprintf(stderr, "ERROR: Input file does not exist: %s\n", in_file_path);
      goto end;
   }

   /* Get output file path, if specified */
   if ((argc > 3) && !string_is_empty(argv[3]))
   {
      strlcpy(out_file_path, argv[3], sizeof(out_file_path));
      path_resolve_realpath(out_file_path, sizeof(out_file_path), true);
   }

   /* If we are compressing/extracting and an
    * output file was not specified, generate a
    * temporary output file path */
   if ((action != RZIP_ACTION_QUERY) &&
       string_is_empty(out_file_path))
   {
      const char *in_file_name = path_basename(in_file_path);
      char in_file_dir[DIR_MAX_LENGTH];

      in_file_dir[0] = '\0';

      fill_pathname_parent_dir(in_file_dir, in_file_path, sizeof(in_file_dir));

      if (string_is_empty(in_file_name))
      {
         fprintf(stderr, "ERROR: Invalid input file: %s\n", in_file_path);
         goto end;
      }

      srand((unsigned int)time(NULL));

      for (;;)
      {
         char tmp_str[10] = {0};

         /* Generate 'random' file name */
         rand_str(tmp_str, sizeof(tmp_str) - 1);
         tmp_str[0] = '.';

         if (!string_is_empty(in_file_dir))
            fill_pathname_join_special(out_file_path, in_file_dir,
                  tmp_str, sizeof(out_file_path));
         else
            strlcpy(out_file_path, tmp_str, sizeof(out_file_path));

         strlcat(out_file_path, ".", sizeof(out_file_path));
         strlcat(out_file_path, in_file_name, sizeof(out_file_path));
         path_resolve_realpath(out_file_path, sizeof(out_file_path), true);

         if (!path_is_valid(out_file_path))
            break;
      }

      in_place = true;
   }

   /* Ensure that input and output files
    * are different */
   if (string_is_equal(in_file_path, out_file_path))
   {
      fprintf(stderr, "ERROR: Input and output are the same file: %s\n", in_file_path);
      goto end;
   }

   /* Get input file size */
   in_file_size = (int64_t)path_get_size(in_file_path);

   if (in_file_size < 1)
   {
      fprintf(stderr, "ERROR: Input file is empty: %s\n", in_file_path);
      goto end;
   }

   /* Open input file
    * > Always use RZIP interface */
   in_file = intfstream_open_rzip_file(
         in_file_path, RETRO_VFS_FILE_ACCESS_READ);

   if (!in_file)
   {
      fprintf(stderr, "ERROR: Failed to open input file: %s\n", in_file_path);
      goto end;
   }

   /* Get input file compression status */
   in_file_compressed = intfstream_is_compressed(in_file);

   /* Get raw (uncompressed) input file size */
   in_file_raw_size   = intfstream_get_size(in_file);

   /* If this is a query operation, just
    * print current state */
   if (action == RZIP_ACTION_QUERY)
   {
      printf("%s: %s\n",
            in_file_compressed ? "File is in RZIP format" : "File is NOT in RZIP format",
            in_file_path);
      printf("   Size on disk:      %" PRIi64 " bytes\n", in_file_size);
      if (in_file_compressed)
         printf("   Uncompressed size: %" PRIi64 " bytes\n", in_file_raw_size);
      goto end;
   }

   /* Check whether file is already in the
    * requested state */
   if ((in_file_compressed  && (action == RZIP_ACTION_COMPRESS)) ||
       (!in_file_compressed && (action == RZIP_ACTION_EXTRACT)))
   {
      printf("Input file is %s: %s\n",
            in_file_compressed ?
                  "already in RZIP format - cannot compress" :
                        "not in RZIP format - cannot extract",
            in_file_path);
      goto end;
   }

   /* Check whether output file already exists */
   if (path_is_valid(out_file_path))
   {
      char reply[8];

      reply[0] = '\0';

      printf("WARNING: Output file already exists: %s\n", out_file_path);
      printf("         Overwrite? [Y/n]: ");
      fgets(reply, sizeof(reply), stdin);
      if (reply[0] != 'Y')
         goto end;
   }

   /* Open output file */
   if (in_file_compressed)
      out_file = intfstream_open_file(
            out_file_path, RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
   else
      out_file = intfstream_open_rzip_file(
            out_file_path, RETRO_VFS_FILE_ACCESS_WRITE);

   if (!out_file)
   {
      fprintf(stderr, "ERROR: Failed to open output file: %s\n", out_file_path);
      goto end;
   }

   /* Start file transfer */
   printf("%s file\n", in_file_compressed ? "Extracting" : "Compressing");
   printf("   From: %s\n", in_file_path);
   printf("   To:   %s\n", in_place ? in_file_path : out_file_path);

   for (;;)
   {
      int64_t data_written = 0;
      uint8_t buffer[FILE_TRANSFER_CHUNK_SIZE];
      /* Read a single chunk from input file */
      int64_t data_read    = intfstream_read(
            in_file, buffer, sizeof(buffer));

      if (data_read < 0)
      {
         fprintf(stderr, "ERROR: Failed to read from input file: %s\n", in_file_path);
         goto end;
      }

      total_data_read += data_read;

      /* Check whether we have reached the end of the file */
      if (data_read == 0)
      {
         /* Close files */
         intfstream_flush(out_file);
         intfstream_close(out_file);
         free(out_file);
         out_file = NULL;

         intfstream_close(in_file);
         free(in_file);
         in_file = NULL;

         break;
      }

      /* Write chunk to backup file */
      data_written = intfstream_write(out_file, buffer, data_read);

      if (data_written != data_read)
      {
         fprintf(stderr, "ERROR: Failed to write to output file: %s\n", out_file_path);
         goto end;
      }

      /* Update progress */
      printf("\rProgress: %" PRIi64 " %%", total_data_read * 100 / in_file_raw_size);
      fflush(stdout);
   }
   printf("\rProgress: 100 %%\n");

   /* Display final status 'report' */
   printf("%s complete:\n", in_file_compressed ? "Extraction" : "Compression");

   out_file_size  = (int64_t)path_get_size(out_file_path);
   file_size_diff = (in_file_size > out_file_size) ?
         (in_file_size - out_file_size) :
               (out_file_size - in_file_size);

   printf("   %" PRIi64 " -> %" PRIi64 " bytes [%" PRIi64 " %% %s]\n",
         in_file_size, out_file_size,
               file_size_diff * 100 / in_file_size,
               (out_file_size >= in_file_size) ?
                     "increase" : "decrease");

   /* If this was an in-place operation,
    * replace input file with output file */
   if (in_place)
   {
      filestream_delete(in_file_path);
      if (filestream_rename(out_file_path, in_file_path))
      {
         fprintf(stderr, "ERROR: Failed to rename temporary file\n");
         fprintf(stderr, "   From: %s\n", out_file_path);
         fprintf(stderr, "   To:   %s\n", in_file_path);
         goto end;
      }
   }

   ret = 0;

end:
   if (in_file)
   {
      intfstream_close(in_file);
      free(in_file);
   }

   if (out_file)
   {
      intfstream_close(out_file);
      free(out_file);
   }

   return ret;
}
