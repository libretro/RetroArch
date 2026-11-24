/* The test joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). The driver source is included
 * for its pad table. Its pads are pressed as a test run presses them:
 * by a script of steps, written to a file and read by the driver's own
 * init, and taken one frame at a time by its poll. */
#include <stdio.h>
#include <unistd.h>

/* answered here, where stubs.c answers them for the other tests */
#define filestream_read_file             test_filestream_read_file
#define input_autoconfigure_connect_uniq test_autoconfigure_connect_uniq

#include "../../../input/drivers_joypad/test_joypad.c"

#include "bridge_view.h"

#define FRAMES 2000

static char     script_path[64];
static uint64_t frame_now;
static unsigned plugged;

const char *input_config_get_test_input_file(bool joypad)
{
   (void)joypad;
   return script_path;
}

uint64_t video_driver_get_frame_count(void) { return frame_now; }

bool input_autoconfigure_connect_uniq(const char *name,
      const char *display_name, const char *phys, const char *uniq,
      const char *driver, unsigned port, unsigned vid, unsigned pid)
{
   (void)name; (void)display_name; (void)phys; (void)uniq;
   (void)driver; (void)vid; (void)pid;
   plugged |= 1u << port;
   return true;
}

bool input_autoconfigure_pending(void) { return false; }

/* the script, read whole as the driver reads it */
int64_t filestream_read_file(const char *path, void **buf, int64_t *len)
{
   long n;
   FILE *fp = fopen(path, "rb");
   *buf     = NULL;
   *len     = 0;
   if (!fp)
      return 0;
   fseek(fp, 0, SEEK_END);
   n = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   if (n < 0 || !(*buf = malloc((size_t)n + 1)))
   {
      fclose(fp);
      return 0;
   }
   *len = (int64_t)fread(*buf, 1, (size_t)n, fp);
   ((char*)*buf)[*len] = '\0';
   fclose(fp);
   return 1;
}

bool path_is_valid(const char *path) { return access(path, F_OK) == 0; }

void input_driver_platform_request(enum input_platform_request req)
{
   (void)req;
}

/* what the script has done to each pad by each frame */
static uint32_t want_buttons[FRAMES][DEFAULT_MAX_PADS];
static int16_t  want_axes[FRAMES][DEFAULT_MAX_PADS][TEST_JOYPAD_AXES];

/* Writes a script: pads plugged into ports 0 and 1, and into ports past
 * the driver's, which it skips; then a press, release or axis move a
 * frame on a random pad. */
static int write_script(void)
{
   unsigned f, p, a;
   uint32_t buttons[DEFAULT_MAX_PADS];
   int16_t  axes[DEFAULT_MAX_PADS][TEST_JOYPAD_AXES];
   FILE *fp = fopen(script_path, "w");

   if (!fp)
      return 0;
   memset(buttons, 0, sizeof(buttons));
   memset(axes, 0, sizeof(axes));
   fprintf(fp, "[\n"
         "{\"frame\":0,\"action\":1,\"param_num\":0,\"param_str\":\"(1111:0001) Red\"},\n"
         "{\"frame\":0,\"action\":1,\"param_num\":%u,\"param_str\":\"Nowhere\"},\n"
         "{\"frame\":0,\"action\":1,\"param_num\":4000000,\"param_str\":\"Far\"},\n"
         "{\"frame\":0,\"action\":1,\"param_num\":1,\"param_str\":\"Blue\"},\n"
         "{\"frame\":1,\"action\":2,\"param_num\":%u}",
         (unsigned)MAX_USERS + 3, (unsigned)DEFAULT_MAX_PADS);
   for (f = 1; f < FRAMES; f++)
   {
      if (f < MAX_TEST_STEPS - 6)
      {
         unsigned r = bv_rand() % 3;
         p          = bv_rand() % DEFAULT_MAX_PADS;
         if (r == 0)
         {
            uint32_t m  = bv_rand();
            buttons[p] |= m;
            fprintf(fp, ",\n{\"frame\":%u,\"action\":%u,\"param_num\":%u}",
                  f, JOYPAD_TEST_COMMAND_BUTTON_PRESS_FIRST + p, m);
         }
         else if (r == 1)
         {
            uint32_t m  = bv_rand();
            buttons[p] &= ~m;
            fprintf(fp, ",\n{\"frame\":%u,\"action\":%u,\"param_num\":%u}",
                  f, JOYPAD_TEST_COMMAND_BUTTON_RELEASE_FIRST + p, m);
         }
         else
         {
            int16_t v  = bv_rand_axis();
            a          = bv_rand() % TEST_JOYPAD_AXES;
            axes[p][a] = v;
            fprintf(fp, ",\n{\"frame\":%u,\"action\":%u,\"param_num\":%u}",
                  f, JOYPAD_TEST_COMMAND_BUTTON_AXIS_FIRST
                     + p * TEST_JOYPAD_AXES + a, (unsigned)(uint16_t)v);
         }
      }
      memcpy(want_buttons[f], buttons, sizeof(buttons));
      memcpy(want_axes[f], axes, sizeof(axes));
   }
   fprintf(fp, "\n]\n");
   fclose(fp);
   return 1;
}

/* Runs the script frame by frame. Returns the number of disagreements. */
static unsigned run_script(void)
{
   unsigned f, p, i, bad = 0;

   plugged   = 0;
   frame_now = 0;
   if (!test_joypad.init(NULL))
      return 1;
   if (plugged != 3)
      bad++;
   for (f = 1; f < FRAMES; f++)
   {
      /* a step is taken on the first poll past its frame */
      frame_now = f + 1;
      test_joypad.poll();
      for (p = 0; p < DEFAULT_MAX_PADS; p++)
      {
         for (i = 0; i < 32; i++)
            if (     (test_joypad.button(p, (uint16_t)i) != 0)
                  != (((want_buttons[f][p] >> i) & 1) != 0))
               bad++;
         for (i = 0; i < TEST_JOYPAD_AXES; i++)
            if ((int16_t)(test_joypad.axis(p, AXIS_POS(i))
                     + test_joypad.axis(p, AXIS_NEG(i)))
                  != want_axes[f][p][i])
               bad++;
         if (f % 50 == 0)
         {
            bad += bv_compare(&test_joypad, p);
            bad += bv_compare_view(&test_joypad, p);
         }
      }
   }
   return bad;
}

int main(void)
{
   unsigned bad = 0, p;
   int fd;

   strcpy(script_path, "/tmp/test_checked_XXXXXX");
   if ((fd = mkstemp(script_path)) < 0)
      return 1;
   close(fd);
   if (!write_script())
      return 1;

   bad += run_script();

   /* gone with the driver: the pads, and their names with the steps */
   test_joypad.destroy();
   for (p = 0; p < DEFAULT_MAX_PADS; p++)
      if (     test_joypad.name(p)
            || test_joypad.button(p, 0) || test_joypad.button(p, 1)
            || test_joypad.axis(p, AXIS_POS(0)) || test_joypad.axis(p, AXIS_NEG(0)))
         bad++;

   /* and an init after plays the script again from its start */
   bad += run_script();
   test_joypad.destroy();
   remove(script_path);

   if (bad)
   {
      printf("FAIL test_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS test_checked_test: a %u-frame script, twice; the pads are"
         " what it pressed, and what the bridge would read is what the"
         " driver says\n", FRAMES);
   return 0;
}
