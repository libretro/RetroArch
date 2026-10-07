/* Handing the console to another program.
 *
 * At the top of memory (MEM2 on the Wii, MEM1 on the GameCube) go,
 * from the top down: the command line, the loader stub with its plan,
 * and the image.  Everything the program loads must end below that,
 * so the stub copies without overlap. */

#include <errno.h>
#include <string.h>

#include <gekko/exec.h>

#include "exec_image.h"
#include "kernel.h"

#define ARGV_MAGIC 0x5f617267u
#define LINE_MAX_  4096u
#define STUB_AREA  4096u
#define PROGRAM_LO 0x80003100u

extern uint8_t __gk_image_end[];
extern uint8_t gk_exec_stub[], gk_exec_stub_end[];
void gk_exec_switch(void (*fn)(void), void *stack_top);

/* In this program's own image, which the copies never reach; so is
 * the stack they run on, since the caller's may be in the heap. */
static struct
{
   const void *image;
   uint32_t    len, top, stage, stub_at, stub_len, plan_at, args_at;
   uint32_t    line_len, argv_off;
} job;
static struct gk_exec_plan plan;
static char                line[LINE_MAX_];
static uint64_t            stack[256];

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

/* Interrupts off, on our own stack: from here the heap, and anything
 * in it, may be overwritten. */
static void finish(void)
{
   uint8_t *stage = (uint8_t*)job.stage;
   uint32_t i;
   memmove(stage, job.image, job.len);
   memcpy((void*)job.stub_at, gk_exec_stub, job.stub_len);
   for (i = 0; i < plan.count; i++)
      plan.seg[i].src += job.stage;
   memcpy((void*)job.plan_at, &plan, sizeof(plan));
   memcpy((void*)job.args_at, line, job.line_len);
   if (job.argv_off)
   {
      uint8_t *b = stage + job.argv_off;
      put32(b,      ARGV_MAGIC);
      put32(b + 4,  job.args_at);
      put32(b + 8,  job.line_len);
      put32(b + 12, 0);
      put32(b + 16, 0);
      put32(b + 20, 0);
   }
   gk_dcache_flush(stage, job.top - job.stage);
   gk_icache_invalidate((void*)job.stub_at, job.stub_len);
   ((void (*)(void*))job.stub_at)((void*)job.plan_at);
}

int gk_exec(const void *image, size_t len, int argc, const char *const *argv)
{
   struct gk_exec_mem mem;
   uint32_t stub_len = (uint32_t)(gk_exec_stub_end - gk_exec_stub);
   uint32_t top, args_at, stub_at, plan_at, stage, floor_, argv_off;
   uint32_t line_len = 0;
   int      n;

   gk_mem_bounds(&mem.mem1_hi, &mem.mem2_lo, &mem.mem2_hi);
   mem.mem1_lo = PROGRAM_LO;
   if (GK_RVL)
   {
      top    = mem.mem2_hi;
      floor_ = mem.mem2_lo;
   }
   else
   {
      top    = mem.mem1_hi;
      floor_ = ((uint32_t)__gk_image_end + 31) & ~31u;
   }
   args_at = top - LINE_MAX_;
   stub_at = args_at - STUB_AREA;
   plan_at = stub_at + ((stub_len + 31) & ~31u);
   if (plan_at + sizeof(plan) > args_at || len > stub_at - floor_)
      return -ENOEXEC;
   stage = (uint32_t)(stub_at - len) & ~31u;
   /* What the program loads stays below the stage. */
   if (GK_RVL)
      mem.mem2_hi = stage;
   else
      mem.mem1_hi = stage;
   if (gk_exec_parse((const uint8_t*)image, len, &mem, &plan))
      return -ENOEXEC;

   for (n = 0; n < argc; n++)
   {
      size_t l = strlen(argv[n]) + 1;
      if (l > LINE_MAX_ - line_len)
         return -E2BIG;
      memcpy(line + line_len, argv[n], l);
      line_len += l;
   }
   argv_off = argc > 0 ? gk_exec_argv_offset((const uint8_t*)image, &plan)
                       : 0;

   job.image    = image;
   job.len      = len;
   job.top      = top;
   job.stage    = stage;
   job.stub_at  = stub_at;
   job.stub_len = stub_len;
   job.plan_at  = plan_at;
   job.args_at  = args_at;
   job.line_len = line_len;
   job.argv_off = argv_off;
   gk_shutdown();
   gk_quiesce();
   gk_exec_switch(finish, stack + sizeof(stack) / sizeof(stack[0]));
   for (;;)
      ;
}
