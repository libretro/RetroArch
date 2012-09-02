#include "scaler.h"
#include <Imlib2.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>

static float g_horiz_scale = 1.0f;
static float g_vert_scale  = 1.0f;

static enum scaler_type g_scaler_type  = SCALER_TYPE_SINC;

static char *g_in_path;
static char *g_out_path;

static void print_help(void)
{
   fprintf(stderr, "Usage: scale [...options...]\n");
   fprintf(stderr, "\t-i/--input: Input file\n");
   fprintf(stderr, "\t-o/--output: Output file\n");
   fprintf(stderr, "\t-x/--xscale: Relative scale in X\n");
   fprintf(stderr, "\t-y/--yscale: Relative scale in Y\n");
   fprintf(stderr, "\t-s/--scale: Relative scale in both X/Y\n");
   fprintf(stderr, "\t-t/--type: Filter type. Valid ones are:\n");
   fprintf(stderr, "\t\tsinc, point, bilinear\n");
   fprintf(stderr, "\t-h/--help: Prints this help\n");
}

static bool parse_args(int argc, char *argv[])
{
   const struct option opts[] = {
      { "xscale", 1, NULL, 'x' },
      { "yscale", 1, NULL, 'y' },
      { "scale", 1, NULL, 's' },
      { "input", 1, NULL, 'i' },
      { "output", 1, NULL, 'o' },
      { "type", 1, NULL, 't' },
      { "help", 0, NULL, 'h' },
      { NULL, 0, NULL, 0 },
   };

   const char *optstring = "x:y:i:o:t:s:h";

   for (;;)
   {
      int c = getopt_long(argc, argv, optstring, opts, NULL);
      if (c == -1)
         break;

      switch (c)
      {
         case 'h':
            print_help();
            exit(EXIT_SUCCESS);

         case 's':
            g_horiz_scale = strtof(optarg, NULL);
            g_vert_scale  = g_horiz_scale;
            break;

         case 'x':
            g_horiz_scale = strtof(optarg, NULL);
            break;

         case 'y':
            g_vert_scale = strtof(optarg, NULL);
            break;

         case 'i':
            g_in_path = strdup(optarg);
            break;

         case 'o':
            g_out_path = strdup(optarg);
            break;

         case '?':
            print_help();
            return false;

         case 't':
            if (strcmp(optarg, "sinc") == 0)
               g_scaler_type = SCALER_TYPE_SINC;
            else if (strcmp(optarg, "bilinear") == 0)
               g_scaler_type = SCALER_TYPE_BILINEAR;
            else if (strcmp(optarg, "point") == 0)
               g_scaler_type = SCALER_TYPE_POINT;
            else
            {
               print_help();
               return false;
            }
            break;
      }
   }

   if (!g_in_path || !g_out_path)
   {
      print_help();
      return false;
   }

   if (optind < argc)
   {
      print_help();
      return false;
   }
   
   return true;
}

int main(int argc, char *argv[])
{
   if (!parse_args(argc, argv))
      return EXIT_FAILURE;

   Imlib_Image img = imlib_load_image(g_in_path);
   if (!img)
      return EXIT_FAILURE;

   imlib_context_set_image(img);

   struct scaler_ctx ctx = {0};
   ctx.in_width    = imlib_image_get_width();
   ctx.in_height   = imlib_image_get_height();
   ctx.out_width   = (int)(imlib_image_get_width() * g_horiz_scale);
   ctx.out_height  = (int)(imlib_image_get_height() * g_vert_scale);
   ctx.in_stride   = imlib_image_get_width() * sizeof(uint32_t);
   ctx.out_stride  = (int)(imlib_image_get_width() * g_horiz_scale) * sizeof(uint32_t);
   ctx.in_fmt      = SCALER_FMT_ARGB8888;
   ctx.out_fmt     = SCALER_FMT_ARGB8888;
   ctx.scaler_type = g_scaler_type;

   assert(scaler_ctx_gen_filter(&ctx));

   uint32_t *scale_buf = (uint32_t*)calloc(sizeof(uint32_t), ctx.out_width * ctx.out_height);

   //struct timespec tv[2];
   //clock_gettime(CLOCK_MONOTONIC, &tv[0]);
   scaler_ctx_scale(&ctx, scale_buf, imlib_image_get_data_for_reading_only());
   //clock_gettime(CLOCK_MONOTONIC, &tv[1]);

   //double time_ms = (tv[1].tv_sec - tv[0].tv_sec) * 1000.0 + (tv[1].tv_nsec - tv[0].tv_nsec) / 1000000.0;
   //double ns_per_pix = (1000000.0 * time_ms) / (ctx.out_width * ctx.out_height);
   //printf("Time: %.3lf ms, %.3lf ns / pixel\n", time_ms, ns_per_pix);

   Imlib_Image new_img = imlib_create_image_using_data(ctx.out_width, ctx.out_height,
         scale_buf);

   imlib_free_image();
   imlib_context_set_image(new_img);

   const char *fmt = strrchr(g_out_path, '.');
   if (fmt)
      fmt++;
   else
      fmt = "png";

   imlib_image_set_format(fmt);
   imlib_save_image(g_out_path);
   imlib_free_image();

   free(scale_buf);
   free(g_in_path);
   free(g_out_path);

   scaler_ctx_gen_reset(&ctx);
}

