/* The audio_threaded_pipeline config migration.
 *
 * The setting briefly held off/automatic/on and was written as a
 * number; it is a plain bool again. Bools are always saved as "true"
 * or "false", so a bare number on this key can only have come from
 * the three-way setting, and config_get_bool() would otherwise read
 * its automatic (1) as an explicit on - turning the pipeline on for
 * users who never asked for it, on drivers it was never verified
 * against.
 *
 * These cases pin the mapping and, just as importantly, pin that the
 * forms a current build writes are left alone. The rule under test is
 * the one applied in config_load_file(); it is restated here rather
 * than linked because that function needs most of the frontend to
 * run, so a change to one has to be made in the other - the cases
 * below are what says which behaviour is intended. */

#include <stdio.h>
#include <string.h>

#include <boolean.h>
#include <file/config_file.h>

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* Mirrors config_load_file(): the generic bool pass, then the
 * migration. Returns the value the pipeline setting ends up with;
 * default_val is what an absent or unparsable entry leaves behind. */
static bool resolve(const char *line, bool default_val)
{
   char buf[256];
   config_file_t *conf;
   bool out = default_val;
   bool tmp = false;
   const struct config_entry_list *entry;

   strlcpy(buf, line, sizeof(buf));
   if (!(conf = config_file_new_from_string(buf, NULL)))
   {
      printf("FAIL: could not parse config text\n");
      failures++;
      return default_val;
   }

   /* Generic bool pass. */
   if (config_get_bool(conf, "audio_threaded_pipeline", &tmp))
      out = tmp;

   /* Migration. */
   entry = (const struct config_entry_list*)
         config_get_entry(conf, "audio_threaded_pipeline");
   if (     entry
         && entry->value[0] >= '0' && entry->value[0] <= '9'
         && entry->value[1] == '\0')
      out = (entry->value[0] == '2');

   config_file_free(conf);
   return out;
}

int main(void)
{
   /* The three-way values, as that build wrote them. */
   CHECK(resolve("audio_threaded_pipeline = \"0\"\n", false) == false,
         "legacy off did not resolve to off");
   CHECK(resolve("audio_threaded_pipeline = \"1\"\n", false) == false,
         "legacy automatic resolved to on - the pipeline would be "
         "forced on for a user who never asked for it");
   CHECK(resolve("audio_threaded_pipeline = \"2\"\n", false) == true,
         "legacy on did not resolve to on");

   /* Automatic must not resolve to on even where the default is on,
    * and legacy on must survive the opposite default: the stored
    * choice decides, not the default underneath it. */
   CHECK(resolve("audio_threaded_pipeline = \"1\"\n", true) == false,
         "legacy automatic ignored the stored value");
   CHECK(resolve("audio_threaded_pipeline = \"2\"\n", true) == true,
         "legacy on ignored the stored value");

   /* What a current build writes: untouched by the migration. */
   CHECK(resolve("audio_threaded_pipeline = \"true\"\n", false) == true,
         "\"true\" was not read as on");
   CHECK(resolve("audio_threaded_pipeline = \"false\"\n", true) == false,
         "\"false\" was not read as off");

   /* Absent, empty and multi-character values leave the default. The
    * migration keys on a single digit, so none of these may trip it. */
   CHECK(resolve("audio_sync = \"true\"\n", false) == false,
         "an absent entry changed the default");
   CHECK(resolve("audio_sync = \"true\"\n", true) == true,
         "an absent entry changed the default");
   CHECK(resolve("audio_threaded_pipeline = \"\"\n", true) == true,
         "an empty value changed the default");
   CHECK(resolve("audio_threaded_pipeline = \"12\"\n", false) == false,
         "a multi-digit value tripped the migration");
   CHECK(resolve("audio_threaded_pipeline = \"1x\"\n", false) == false,
         "a trailing character tripped the migration");
   CHECK(resolve("audio_threaded_pipeline = \"9\"\n", true) == false,
         "an out-of-range digit did not resolve to off");

   /* A neighbouring bool must be unaffected by any of this. */
   {
      char buf[256];
      config_file_t *conf;
      bool tmp = false;

      strlcpy(buf, "audio_sync = \"1\"\n", sizeof(buf));
      conf = config_file_new_from_string(buf, NULL);
      CHECK(conf != NULL, "could not parse neighbour config");
      if (conf)
      {
         CHECK(config_get_bool(conf, "audio_sync", &tmp) && tmp,
               "a numeric bool on another key stopped reading as on");
         config_file_free(conf);
      }
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("audio_threaded_pipeline migration: all values resolve as intended\n");
   return 0;
}
