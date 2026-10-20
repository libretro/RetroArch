/* SET_HW_RENDER GLES grants.
 *
 * Cores walk a list of context types, highest first, and keep the
 * first one the frontend accepts, so every GLES version the frontend
 * grants must be one the context can give. Built as three variants:
 *
 *   GLES2 build, device property     (the regular Android flavors)
 *   GLES3.2 build, no property       (OpenXR, desktop GLES)
 *   GLES3 build, no property
 *
 * The shipping request function is included and driven directly. */
#include "../../../runloop.c"
#include <assert.h>
#include <stdio.h>

static unsigned warnings;

#ifdef RUNLOOP_GLES_VERSION_PROPERTY
static const char *prop_value;
static unsigned prop_reads;

int __system_property_get(const char *name, char *value)
{
   prop_reads++;
   assert(strcmp(name, "ro.opengles.version") == 0);
   if (!prop_value)
   {
      value[0] = '\0';
      return 0;
   }
   strcpy(value, prop_value);
   return (int)strlen(value);
}
#endif

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; warnings++; }

static bool req(enum retro_hw_context_type type,
      unsigned major, unsigned minor)
{
   return dynamic_request_hw_context(type, minor, major);
}

/* The version a core probing 3.2, 3.1, 3.0 ends up with: what it
 * would ask EGL for. */
static unsigned probe(void)
{
   if (req(RETRO_HW_CONTEXT_OPENGLES_VERSION, 3, 2))
      return 302;
   if (req(RETRO_HW_CONTEXT_OPENGLES_VERSION, 3, 1))
      return 301;
   if (req(RETRO_HW_CONTEXT_OPENGLES3, 0, 0))
      return 300;
   if (req(RETRO_HW_CONTEXT_OPENGLES2, 0, 0))
      return 200;
   return 0;
}

static void check_build_ceiling(unsigned ceiling)
{
   warnings = 0;
   assert(probe() == ceiling);
   assert(warnings == (ceiling == 302 ? 0U : ceiling == 301 ? 1U : 2U));
   assert(req(RETRO_HW_CONTEXT_OPENGLES_VERSION, 3, 0));
   assert(req(RETRO_HW_CONTEXT_OPENGLES_VERSION, 2, 0));
   assert(!req(RETRO_HW_CONTEXT_OPENGLES_VERSION, 3, 3));
   assert(!req(RETRO_HW_CONTEXT_OPENGLES_VERSION, 4, 0));
}

int main(void)
{
   /* Desktop GL on a GLES build is refused whatever the device has. */
   assert(!req(RETRO_HW_CONTEXT_OPENGL, 0, 0));
   assert(!req(RETRO_HW_CONTEXT_OPENGL_CORE, 3, 3));

#ifdef RUNLOOP_GLES_VERSION_PROPERTY
   /* 3.2 device: the probe stops at 3.2 instead of being walked down
    * to 3.0. */
   prop_value = "196610";
   check_build_ceiling(302);

   /* 3.1 device */
   prop_value = "196609";
   check_build_ceiling(301);

   /* 3.0 device: 3.2 and 3.1 refused, so the core still falls back. */
   prop_value = "196608";
   check_build_ceiling(300);

   /* 2.0 device: GLES3 is refused rather than granted and failed. */
   prop_value = "131072";
   assert(probe() == 200);
   assert(!req(RETRO_HW_CONTEXT_OPENGLES3, 0, 0));

   /* No property or an unreadable one: the build decides, which for
    * a GLES2 build is the 3.0 it already granted. */
   prop_value = NULL;
   check_build_ceiling(300);
   prop_value = "garbage";
   check_build_ceiling(300);
   assert(prop_reads > 0);
#elif defined(HAVE_OPENGLES3_2)
   check_build_ceiling(302);
#elif defined(HAVE_OPENGLES3_1)
   check_build_ceiling(301);
#else
   check_build_ceiling(300);
#endif

   printf("hw_context_request: ok\n");
   return 0;
}
