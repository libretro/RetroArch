/* Exercise ANGLE's backend selection and the resulting context policy. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gfx/common/egl_common.h"
#include <EGL/eglext_angle.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"angle_backend_test:%u: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned attempts, succeeds_at, configurations;

static void test_log(const char *format, ...)
{
   CHECK(format != NULL);
}
#define RARCH_LOG test_log

static bool angle_try_initialize(egl_ctx_data_t *egl, void *display_data,
      const EGLint *attrs, EGLint *major, EGLint *minor)
{
   CHECK(egl && !display_data && attrs && major && minor);
   CHECK(attrs[0] == EGL_PLATFORM_ANGLE_TYPE_ANGLE);
   attempts++;
   if (attempts != succeeds_at)
      return false;
   *major = 1;
   *minor = 5;
   return true;
}

bool egl_init_context_common(egl_ctx_data_t *egl, EGLint *count,
      const EGLint *attrs, egl_accept_config_cb_t accept, void *data)
{
   CHECK(egl && count && attrs && !accept && !data);
   configurations++;
   *count = 1;
   return true;
}

#include "angle_backend_fragment.c"

int main(void)
{
   egl_ctx_data_t egl;
   EGLint major, minor, count, attrs[] = {EGL_NONE};
   unsigned i;
   memset(&egl,0,sizeof(egl));
   for (i = 1; i <= 3; i++)
   {
      succeeds_at = i;
      attempts = configurations = 0;
      CHECK(angle_init_context(&egl,NULL,&major,&minor,&count,attrs,NULL));
      CHECK(attempts == i && configurations == 1 && count == 1);
#ifdef __WINRT__
      CHECK(egl.single_context);
#else
      CHECK(egl.single_context == (i != 2));
#endif
   }
   succeeds_at = 0;
   attempts = configurations = 0;
   CHECK(!angle_init_context(&egl,NULL,&major,&minor,&count,attrs,NULL));
   CHECK(attempts == 3 && configurations == 0);
   puts("ANGLE backend context policy: passed");
   return 0;
}
