/* Kernel internals shared by the C and assembly sides. */

#ifndef GEKKO_KERNEL_H
#define GEKKO_KERNEL_H

/* Register frame offsets; the C structs below are checked against
 * these at compile time. */
#define CTX_GPR(n)   ((n) * 4)
#define CTX_CR       128
#define CTX_LR       132
#define CTX_CTR      136
#define CTX_XER      140
#define CTX_SRR0     144
#define CTX_SRR1     148
#define CTX_SIZE     160

/* Floating-point frame: ps0 as doubles, both halves as singles, then
 * FPSCR (stored by stfd, low word). */
#define FPX_FPR(n)   ((n) * 8)
#define FPX_PS(n)    (256 + (n) * 8)
#define FPX_FPSCR    512
#define FPX_SIZE     520

/* Thread layout: register frame, then the FP frame. */
#define THR_FP       CTX_SIZE

/* Per-processor scratch the vector stubs reach in real mode. */
#define SCR_R3       0
#define SCR_R4       4
#define SCR_R5       8
#define SCR_CR       12
#define SCR_SRR0     16
#define SCR_SRR1     20

#define MSR_KERNEL   0x00001032   /* ME | IR | DR | RI */

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>
#include <reent.h>

#include <gekko/thread.h>
#include <gekko/irq.h>

struct gk_fpctx
{
   double   fpr[32];
   float    ps[32][2];
   double   fpscr;
};

enum gk_tstate
{
   GK_T_READY = 0,
   GK_T_RUNNING,
   GK_T_BLOCKED,
   GK_T_DEAD
};

struct gk_thread
{
   struct gk_ctx      ctx;          /* must stay first */
   struct gk_fpctx    fp;           /* must follow ctx */
   uint64_t           wake_at;      /* timeout deadline, ticks */
   struct _reent      reent;
   struct gk_thread  *next;         /* run queue or futex queue */
   struct gk_thread  *prev;
   struct gk_thread  *tnext;        /* timeout list */
   struct gk_thread  *joiner;
   struct gk_thread  *all_next;
   volatile uint32_t *wait_addr;    /* futex word slept on */
   void              *stack;        /* allocation to free, if ours */
   uint32_t          *stack_lo;     /* canary at the bottom */
   void              *ret;
   gk_thread_fn       fn;
   void              *arg;
   void              *tls[GK_TLS_SLOTS];
   int                wait_result;
   uint8_t            prio;
   uint8_t            state;
   uint8_t            timed;        /* on the timeout list */
   uint8_t            fp_valid;     /* fp holds this thread's state */
   uint8_t            detached;
   uint8_t            exited;
};

extern struct gk_thread *gk_cur;      /* running thread */
extern struct gk_thread *gk_fp_owner; /* whose state the FPRs hold */
extern uint32_t          gk_in_exception;
extern uint32_t          gk_scratch[8];
extern uint8_t           gk_irq_stack_top[];

/* Assembly. */
void gk_vectors_install(void);
void gk_syscall_resched(void);   /* 'sc': let the scheduler run */

/* C side of the exception path: returns the thread to resume. */
struct gk_thread *gk_exception_dispatch(unsigned vector,
      struct gk_thread *t);

/* Scheduler, all with interrupts off. */
void gk_sched_init(void *main_stack_lo);
struct gk_thread *gk_sched_main(void);
struct gk_thread *gk_sched_switch(struct gk_thread *t);
void gk_timer_tick(uint64_t now);
void gk_timer_program(uint64_t now);
void gk_irq_init(void);
void gk_irq_dispatch(void);
void gk_newlib_init(void);
void gk_arena_init(void);
void gk_tls_run_dtors(struct gk_thread *t);
void gk_console_show_now(void);
void gk_exit_after_crash(void);
void gk_irq_block_current(volatile uint32_t *addr);
void gk_set_wall_clock(uint64_t unix_seconds);
void gk_rtc_sync(void);
void gk_usbgecko_probe(void);
/* A line of output (without its newline) to Dolphin's log, the
 * console and a USB Gecko, with interrupts off. */
void gk_debug_line(const char *s);

/* Run when the program leaves, by exit() or gk_exec(), last added
 * first; each hook is added once. */
struct gk_exit_hook
{
   void (*fn)(void);
   struct gk_exit_hook *next;
};
void gk_exit_hook_add(struct gk_exit_hook *h);
/* The hooks, then the system's own: interrupts still on. */
void gk_shutdown(void);
/* Interrupts off, every source masked. */
void gk_quiesce(void);
/* The banks as the loader left them, before any arena use. */
void gk_mem_bounds(uint32_t *mem1_end, uint32_t *mem2_lo, uint32_t *mem2_hi);
#if GK_RVL
/* cpu/l2.S: memory up to the two ends is written back first. */
void gk_l2_enhance(uint32_t mem1_end, uint32_t mem2_end);
#endif

#endif
#endif
