/* os/wiiu/system/exception_handler.c against stand-in coreinit: the DSI,
 * ISI and program handlers are installed for every core (global
 * all-cores mode), a fault produces one fatal report, and a second
 * fault while that report is being made parks its thread rather than
 * writing into the same buffer. The single-core OSSetExceptionCallback
 * is deliberately not provided, so using it again fails the link. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <wiiu/types.h>
#include <wiiu/os/thread.h>
#include <wiiu/os/exception.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

struct install
{
   OSExceptionMode       mode;
   OSExceptionType       type;
   OSExceptionCallbackFn cb;
};

static struct install installs[16];
static int     installs_n;
static jmp_buf escape;
static int     fatals;
static int     suspends;
static char    fatal_msg[256];

unsigned int __code_start;
unsigned int __code_end;

OSExceptionCallbackFn OSSetExceptionCallbackEx(OSExceptionMode mode,
      OSExceptionType type, OSExceptionCallbackFn cb)
{
   if (installs_n < 16)
   {
      installs[installs_n].mode = mode;
      installs[installs_n].type = type;
      installs[installs_n].cb   = cb;
      installs_n++;
   }
   return NULL;
}

void OSFatal(const char *msg)
{
   fatals++;
   strncpy(fatal_msg, msg, sizeof(fatal_msg) - 1);
   longjmp(escape, 1);
}

uint32_t OSSuspendThread(OSThread *thread)
{
   (void)thread;
   suspends++;
   longjmp(escape, 2);
   return 0;
}

OSThread *OSGetCurrentThread(void) { return NULL; }
uint32_t OSEffectiveToPhysical(void *addr) { return addr ? 1 : 0; }
void *OSGetSymbolName(u32 addr, char *out, u32 out_size)
{
   (void)addr;
   if (out_size)
      out[0] = '\0';
   return out;
}
int OSDynLoad_Acquire(const char *name, void **handle)
{
   (void)name;
   *handle = NULL;
   return -1;
}
int OSDynLoad_FindExport(void *handle, int is_data, const char *name,
      void **addr)
{
   (void)handle; (void)is_data; (void)name;
   *addr = NULL;
   return -1;
}
void OSDynLoad_Release(void *handle) { (void)handle; }

#include "../../../os/wiiu/system/exception_handler.c"

static OSExceptionCallbackFn installed(OSExceptionType type)
{
   int i;
   for (i = 0; i < installs_n; i++)
      if (installs[i].type == type)
         return installs[i].cb;
   return NULL;
}

int main(void)
{
   int i;
   OSContext ctx;
   OSExceptionCallbackFn dsi;

   setup_os_exceptions();

   CHECK(installs_n == 3);
   for (i = 0; i < installs_n; i++)
      CHECK(installs[i].mode == OS_EXCEPTION_MODE_GLOBAL_ALL_CORES);
   CHECK(installed(OS_EXCEPTION_TYPE_DSI)     != NULL);
   CHECK(installed(OS_EXCEPTION_TYPE_ISI)     != NULL);
   CHECK(installed(OS_EXCEPTION_TYPE_PROGRAM) != NULL);

   /* First fault: one report, through OSFatal. */
   memset(&ctx, 0, sizeof(ctx));
   ctx.srr0 = 0x00000010;
   dsi      = installed(OS_EXCEPTION_TYPE_DSI);
   if (dsi && setjmp(escape) == 0)
      dsi(&ctx);
   CHECK(fatals == 1);
   CHECK(suspends == 0);
   CHECK(strncmp(fatal_msg, "DSI:", 4) == 0);

   /* A fault on another core while that report is up parks. */
   if (dsi && setjmp(escape) == 0)
      dsi(&ctx);
   CHECK(fatals == 1);
   CHECK(suspends == 1);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_exception_cores: ok\n");
   return 0;
}
