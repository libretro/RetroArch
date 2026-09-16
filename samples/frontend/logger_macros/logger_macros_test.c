/* The HAVE_LOGGER va_list macros ship one formatted line.
 *
 * Compiles verbosity.h under the logger configurations that a
 * desktop build never sees and drives the *_V macros against
 * capturing sinks:
 *
 *   ORBIS (either salamander flavour): debugNetPrintf is
 *   printf-style with no va_list variant. The old macros handed it
 *   (tag, fmt, vp) - the tag became the format string and a raw
 *   va_list rode as a value argument - and mapped RARCH_WARN_V to a
 *   different level than RARCH_WARN. The sink must now receive one
 *   call per line, at the level matching the non-V spelling, with
 *   the body formatted.
 *
 *   logger_send (non-ORBIS): the old macros were two calls - prefix
 *   plus tag, then body - which interleave under concurrent
 *   writers. One call per line now, prefix, tag and formatted body
 *   together, and the "[WARN] :: " oddity gone.
 *
 * Build selects the region with -DIS_SALAMANDER / -DORBIS; the
 * build script runs all four combinations.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define HAVE_LOGGER 1
#include "../../../verbosity.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* Capturing sinks: every call lands here as one formatted string
 * plus, for debugnet, its level. */
static unsigned sink_calls = 0;
static int      sink_level = -1;
static char     sink_text[600];

#ifdef ORBIS
void debugNetPrintf(int level, const char *fmt, ...)
{
   va_list ap;
   sink_calls++;
   sink_level = level;
   va_start(ap, fmt);
   vsnprintf(sink_text, sizeof(sink_text), fmt, ap);
   va_end(ap);
}
#else
void logger_init(void) { }
void logger_shutdown(void) { }
void logger_send(const char *fmt, ...)
{
   va_list ap;
   sink_calls++;
   va_start(ap, fmt);
   vsnprintf(sink_text, sizeof(sink_text), fmt, ap);
   va_end(ap);
}
void logger_send_v(const char *fmt, va_list ap)
{
   /* The interleave-prone second call of the old shape: with the
    * fix, nothing reaches it from the *_V macros. */
   sink_calls++;
   vsnprintf(sink_text, sizeof(sink_text), fmt, ap);
}
#endif

static void reset(void)
{
   sink_calls   = 0;
   sink_level   = -1;
   sink_text[0] = '\0';
}

/* The macros take a va_list; these build one the way the function
 * wrappers do. The tag is baked in as a literal: the logger macros
 * paste it into a string constant, so a runtime tag never compiled
 * against them. */
static void drive_log_v(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   RARCH_LOG_V("[Video]", fmt, ap);
   va_end(ap);
}

static void drive_warn_v(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
#ifdef ORBIS
   RARCH_WARN_V("[Audio]", fmt, ap);
#else
   RARCH_WARN_V("[Input]", fmt, ap);
#endif
   va_end(ap);
}

static void drive_err_v(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
#ifdef ORBIS
   RARCH_ERR_V("[Core]", fmt, ap);
#else
   RARCH_ERR_V("[Config]", fmt, ap);
#endif
   va_end(ap);
}

int main(void)
{
   /* One call, formatted body present, tag present. */
   reset();
   drive_log_v("mode %ux%u set\n", 640u, 480u);
   CHECK(sink_calls == 1,
         "LOG_V made %u sink calls, want exactly 1", sink_calls);
   CHECK(strstr(sink_text, "640x480") != NULL /* %ux%u */,
         "LOG_V body not formatted: \"%.80s\"", sink_text);
   CHECK(strstr(sink_text, "[Video]") != NULL,
         "LOG_V tag missing: \"%.80s\"", sink_text);

#ifdef ORBIS
   CHECK(sink_level == DEBUGNET_INFO,
         "LOG_V level %d, want DEBUGNET_INFO", sink_level);

   /* WARN_V at the same level as RARCH_WARN. */
   reset();
   RARCH_WARN("plain warn\n");
   {
      int warn_level = sink_level;
      reset();
      drive_warn_v("underrun of %d frames\n", 32);
      CHECK(sink_calls == 1,
            "WARN_V made %u sink calls, want 1", sink_calls);
      CHECK(sink_level == warn_level,
            "WARN_V level %d differs from RARCH_WARN's %d",
            sink_level, warn_level);
   }

   reset();
   drive_err_v("load failed: %s\n", "missing file");
   CHECK(sink_level == DEBUGNET_ERROR,
         "ERR_V level %d, want DEBUGNET_ERROR", sink_level);
   CHECK(strstr(sink_text, "missing file") != NULL,
         "ERR_V body not formatted: \"%.80s\"", sink_text);
#else
   /* No " :: " residue, and the error prefix rides the one call. */
   reset();
   drive_warn_v("pad %d gone\n", 2);
   CHECK(sink_calls == 1,
         "WARN_V made %u sink calls, want 1", sink_calls);
   CHECK(strstr(sink_text, " :: ") == NULL,
         "WARN_V still carries the :: oddity: \"%.80s\"", sink_text);
   CHECK(strstr(sink_text, "pad 2 gone") != NULL,
         "WARN_V body not formatted: \"%.80s\"", sink_text);

   reset();
   drive_err_v("parse error at %d\n", 7);
   CHECK(sink_calls == 1,
         "ERR_V made %u sink calls, want 1", sink_calls);
   CHECK(strstr(sink_text, "[ERROR]") != NULL,
         "ERR_V prefix missing: \"%.80s\"", sink_text);
   CHECK(strstr(sink_text, "parse error at 7") != NULL,
         "ERR_V body not formatted: \"%.80s\"", sink_text);
#endif

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("logger_macros: all lanes passed\n");
   return 0;
}
