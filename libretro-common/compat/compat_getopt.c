/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (compat_getopt.c).
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
#include <ctype.h>

#include <string.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>

#include <retro_miscellaneous.h>

#include <compat/getopt.h>
#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/posix_string.h>

char *optarg;
int optind, opterr, optopt;

static bool is_short_option(const char *str)
{
   return str[0] == '-' && str[1] != '-';
}

static bool is_long_option(const char *str)
{
   return str[0] == '-' && str[1] == '-';
}

static int find_short_index(char * const *argv)
{
   int idx;
   for (idx = 0; argv[idx]; idx++)
   {
      if (is_short_option(argv[idx]))
         return idx;
   }

   return -1;
}

static int find_long_index(char * const *argv)
{
   int idx;
   for (idx = 0; argv[idx]; idx++)
   {
      if (is_long_option(argv[idx]))
         return idx;
   }

   return -1;
}

static int parse_short(const char *optstring, char * const *argv)
{
   bool extra_opt, takes_arg, embedded_arg;
   const char *opt = NULL;
   char        arg = argv[0][1];

   if (arg == ':')
      return '?';

   opt = strchr(optstring, arg);
   if (!opt)
      return '?';

   extra_opt = argv[0][2];
   takes_arg = opt[1] == ':';

   /* If we take an argument, and we see additional characters,
    * this is in fact the argument (i.e. -cfoo is same as -c foo). */
   embedded_arg = extra_opt && takes_arg;

   if (takes_arg)
   {
      if (embedded_arg)
      {
         optarg = argv[0] + 2;
         optind++;
      }
      else
      {
         optarg = argv[1];
         optind += 2;
      }

      return optarg ? opt[0] : '?';
   }

   if (embedded_arg)
   {
      /* If we see additional characters,
       * and they don't take arguments, this
       * means we have multiple flags in one. */
      memmove(&argv[0][1], &argv[0][2], strlen(&argv[0][2]) + 1);
      return opt[0];
   }

   optind++;
   return opt[0];
}

static int parse_long(const struct option *longopts, char * const *argv)
{
   size_t indice;
   char *save  = NULL;
   char *argv0 = strdup(&argv[0][2]);
   char *token = strtok_r(argv0, "=", &save);
   const struct option *opt = NULL;

   for (indice = 0; longopts[indice].name; indice++)
   {
      if (token && !strcmp(longopts[indice].name, token))
      {
         opt = &longopts[indice];
         break;
      }
   }

   free(argv0);
   argv0 = NULL;

   if (!opt)
      return '?';

   /* Handle args with '=' instead of space */
   if (opt->has_arg)
   {
      char *special_arg = strchr(argv[0], '=');
      if (special_arg)
      {
         optarg = ++special_arg;
         optind++;
         return opt->val;
      }
   }

   /* getopt_long has an "optional" arg, but we don't bother with that. */
   if (opt->has_arg && !argv[1])
      return '?';

   if (opt->has_arg)
   {
      optarg = argv[1];
      optind += 2;
   }
   else
      optind++;

   if (opt->flag)
   {
      *opt->flag = opt->val;
      return 0;
   }

   return opt->val;
}

static void shuffle_block(char **begin, char **last, char **end)
{
   ptrdiff_t    len = last - begin;
   const char **tmp = (const char**)calloc(len, sizeof(const char*));

   memcpy((void*)tmp, begin, len * sizeof(const char*));
   memmove(begin, last, (end - last) * sizeof(const char*));
   memcpy(end - len, tmp, len * sizeof(const char*));

   free((void*)tmp);
}

int getopt_long(int argc, char *argv[],
      const char *optstring, const struct option *longopts, int *longindex)
{
   int short_index, long_index;

   if (optind == 0)
      optind = 1;

   if (argc < 2)
      return -1;

   short_index = find_short_index(&argv[optind]);
   long_index  = find_long_index(&argv[optind]);

   /* We're done here. */
   if (short_index == -1 && long_index == -1)
      return -1;

   /* Reorder argv so that non-options come last.
    * Non-POSIXy, but that's what getopt does by default. */
   if ((short_index > 0) && ((short_index < long_index) || (long_index == -1)))
   {
      shuffle_block(&argv[optind], &argv[optind + short_index], &argv[argc]);
      short_index = 0;
   }
   else if ((long_index > 0) && ((long_index < short_index)
            || (short_index == -1)))
   {
      shuffle_block(&argv[optind], &argv[optind + long_index], &argv[argc]);
      long_index = 0;
   }

   if (short_index == 0)
      return parse_short(optstring, &argv[optind]);
   if (long_index == 0)
      return parse_long(longopts, &argv[optind]);

   return '?';
}
