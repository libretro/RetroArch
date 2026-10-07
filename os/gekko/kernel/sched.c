/* Scheduler, timeouts and the futex queues everything blocks on.
 *
 * Everything here runs with external interrupts off: on one processor
 * that is the whole locking story.  A thread that has to give up the
 * processor issues 'sc'; the exception path saves it, and on the way
 * out gk_sched_switch() decides who runs next. */

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"

#define SLICE_US     5000
#define FUTEX_BUCKETS 64
#define STACK_CANARY 0x5afe57acu
#define IDLE_STACK   2048

struct gk_thread *gk_cur;
struct gk_thread *gk_fp_owner;
uint32_t          gk_in_exception;
uint32_t          gk_tb_hz;

static struct gk_thread *ready_head[GK_PRIO_MAX + 1];
static struct gk_thread *ready_tail[GK_PRIO_MAX + 1];
static uint32_t          ready_map[(GK_PRIO_MAX + 1) / 32];
static struct gk_thread *futex_q[FUTEX_BUCKETS];
static struct gk_thread *timer_head;
static struct gk_thread *zombies;
static struct gk_thread *all_threads;
static uint64_t          slice_end;
static uint32_t          yield_req;

static struct gk_thread  main_thread;
static struct gk_thread  idle_thread;
static uint32_t          idle_stack[IDLE_STACK / 4];

/* ---- run queues ---- */

static int ready_top(void)
{
   int i;
   for (i = (GK_PRIO_MAX + 1) / 32 - 1; i >= 0; i--)
      if (ready_map[i])
         return i * 32 + 31 - __builtin_clz(ready_map[i]);
   return -1;
}

static void rq_push(struct gk_thread *t, int at_head)
{
   unsigned p = t->prio;
   t->state = GK_T_READY;
   t->prev  = NULL;
   t->next  = NULL;
   if (!ready_head[p])
   {
      ready_head[p] = ready_tail[p] = t;
      ready_map[p >> 5] |= 1u << (p & 31);
   }
   else if (at_head)
   {
      t->next = ready_head[p];
      ready_head[p]->prev = t;
      ready_head[p] = t;
   }
   else
   {
      t->prev = ready_tail[p];
      ready_tail[p]->next = t;
      ready_tail[p] = t;
   }
}

static void rq_remove(struct gk_thread *t)
{
   unsigned p = t->prio;
   if (t->prev)
      t->prev->next = t->next;
   else
      ready_head[p] = t->next;
   if (t->next)
      t->next->prev = t->prev;
   else
      ready_tail[p] = t->prev;
   if (!ready_head[p])
      ready_map[p >> 5] &= ~(1u << (p & 31));
   t->next = t->prev = NULL;
}

/* ---- timeouts ---- */

static void timer_remove(struct gk_thread *t)
{
   struct gk_thread **pp = &timer_head;
   if (!t->timed)
      return;
   while (*pp && *pp != t)
      pp = &(*pp)->tnext;
   if (*pp)
      *pp = t->tnext;
   t->tnext = NULL;
   t->timed = 0;
}

static void timer_insert(struct gk_thread *t, uint64_t at)
{
   struct gk_thread **pp = &timer_head;
   t->wake_at = at;
   while (*pp && (*pp)->wake_at <= at)
      pp = &(*pp)->tnext;
   t->tnext = *pp;
   *pp      = t;
   t->timed = 1;
}

void gk_timer_program(uint64_t now)
{
   uint64_t next = (uint64_t)-1;
   uint64_t delta;
   if (timer_head)
      next = timer_head->wake_at;
   if (gk_cur && ready_head[gk_cur->prio] && slice_end < next)
      next = slice_end;
   if (next == (uint64_t)-1 || next - now > 0x7fffffffu)
      delta = 0x7fffffff;
   else if (next <= now)
      delta = 1;
   else
      delta = next - now;
   __asm__ __volatile__("mtdec %0" : : "r"((uint32_t)delta));
}

/* ---- futex queues: per bucket, by priority, FIFO among equals ---- */

static unsigned fq_bucket(volatile uint32_t *addr)
{
   uint32_t a = (uint32_t)addr >> 2;
   return (a ^ (a >> 6) ^ (a >> 12)) & (FUTEX_BUCKETS - 1);
}

static void fq_insert(struct gk_thread *t, volatile uint32_t *addr)
{
   struct gk_thread **pp = &futex_q[fq_bucket(addr)];
   struct gk_thread  *prev = NULL;
   t->wait_addr = addr;
   while (*pp && (*pp)->prio >= t->prio)
   {
      prev = *pp;
      pp   = &(*pp)->next;
   }
   t->next = *pp;
   t->prev = prev;
   if (*pp)
      (*pp)->prev = t;
   *pp = t;
}

static void fq_remove(struct gk_thread *t)
{
   unsigned b = fq_bucket(t->wait_addr);
   if (t->prev)
      t->prev->next = t->next;
   else
      futex_q[b] = t->next;
   if (t->next)
      t->next->prev = t->prev;
   t->next = t->prev = NULL;
   t->wait_addr = NULL;
}

/* First (highest priority) sleeper on addr, or NULL. */
static struct gk_thread *fq_first(volatile uint32_t *addr)
{
   struct gk_thread *t = futex_q[fq_bucket(addr)];
   while (t && t->wait_addr != addr)
      t = t->next;
   return t;
}

/* Make a sleeper runnable with a result. */
static void wake_one(struct gk_thread *t, int result)
{
   fq_remove(t);
   timer_remove(t);
   t->wait_result = result;
   rq_push(t, 0);
}

/* Interrupts off.  Put the running thread to sleep on addr; returns
 * the wake result once it runs again. */
static int block_on(volatile uint32_t *addr, uint64_t timeout)
{
   struct gk_thread *self = gk_cur;
   if (timeout == 0)
      return GK_ETIMEDOUT;
   self->state       = GK_T_BLOCKED;
   self->wait_result = GK_OK;
   fq_insert(self, addr);
   if (timeout != GK_WAIT_FOREVER)
      timer_insert(self, gk_ticks() + timeout);
   gk_syscall_resched();
   return self->wait_result;
}

/* Interrupts off.  Give the processor up if a woken thread outranks
 * the running one; from a handler the exit path does it. */
static void maybe_preempt(void)
{
   if (gk_in_exception)
      return;
   if (ready_top() > (int)gk_cur->prio)
      gk_syscall_resched();
   else
      gk_timer_program(gk_ticks());
}

/* Exception context: the interrupted thread sleeps on addr until a
 * wake, the next thread runs when the handler returns. */
void gk_irq_block_current(volatile uint32_t *addr)
{
   struct gk_thread *t = gk_cur;
   if (!gk_in_exception || t->state != GK_T_RUNNING || t->prio == GK_PRIO_IDLE)
      return;
   t->state       = GK_T_BLOCKED;
   t->wait_result = GK_OK;
   fq_insert(t, addr);
}

/* ---- the switch ---- */

void gk_timer_tick(uint64_t now)
{
   while (timer_head && timer_head->wake_at <= now)
   {
      struct gk_thread *t = timer_head;
      timer_head = t->tnext;
      t->tnext   = NULL;
      t->timed   = 0;
      if (t->wait_addr)
         fq_remove(t);
      t->wait_result = GK_ETIMEDOUT;
      rq_push(t, 0);
   }
   if (gk_cur && now >= slice_end && ready_head[gk_cur->prio])
      yield_req = 1;
}

static void check_stack(struct gk_thread *t)
{
   if (t->stack_lo && *t->stack_lo != STACK_CANARY)
      gk_panic("stack overflow in thread %p", (void*)t);
}

/* Exception exit: choose who runs.  t is the thread that was
 * interrupted (gk_cur). */
struct gk_thread *gk_sched_switch(struct gk_thread *t)
{
   struct gk_thread *next;
   int top = ready_top();

   if (t->state == GK_T_RUNNING)
   {
      if (top > (int)t->prio)
         rq_push(t, 1);
      else if (yield_req && top == (int)t->prio)
         rq_push(t, 0);
      else
      {
         yield_req = 0;
         gk_timer_program(gk_ticks());
         goto resume;
      }
   }
   check_stack(t);
   yield_req = 0;
   top  = ready_top();
   next = ready_head[top];
   rq_remove(next);
   next->state = GK_T_RUNNING;
   if (next != t)
      slice_end = gk_ticks() + GK_US_TO_TICKS(SLICE_US);
   gk_cur = next;
   t      = next;
   gk_timer_program(gk_ticks());
resume:
   if (gk_fp_owner == t)
      t->ctx.srr1 |= GK_MSR_FP;
   else
      t->ctx.srr1 &= ~GK_MSR_FP;
   return t;
}

/* ---- threads ---- */

static void thread_entry(struct gk_thread *t);
static void free_zombies(void);

static void ctx_setup(struct gk_thread *t, uint32_t *stack_top)
{
   uint32_t r2, r13;
   uint32_t *sp = (uint32_t*)((uint32_t)stack_top & ~15u) - 4;
   sp[0] = 0;
   sp[1] = 0;
   __asm__ __volatile__("mr %0,2" : "=r"(r2));
   __asm__ __volatile__("mr %0,13" : "=r"(r13));
   memset(&t->ctx, 0, sizeof(t->ctx));
   t->ctx.gpr[1]  = (uint32_t)sp;
   t->ctx.gpr[2]  = r2;
   t->ctx.gpr[3]  = (uint32_t)t;
   t->ctx.gpr[13] = r13;
   t->ctx.srr0    = (uint32_t)thread_entry;
   t->ctx.srr1    = MSR_KERNEL | GK_MSR_EE;
}

/* Never blocks: it is what runs when nothing else can. */
static void idle_loop(struct gk_thread *t)
{
   (void)t;
   for (;;)
      ;
}

void gk_sched_init(void *main_stack_lo)
{
   struct gk_thread *m = &main_thread;
   memset(m, 0, sizeof(*m));
   m->prio     = GK_PRIO_DEFAULT;
   m->state    = GK_T_RUNNING;
   m->stack_lo = (uint32_t*)main_stack_lo;
   *m->stack_lo = STACK_CANARY;
   m->detached = 1;
   all_threads = m;
   gk_cur      = m;

   memset(&idle_thread, 0, sizeof(idle_thread));
   ctx_setup(&idle_thread, idle_stack + IDLE_STACK / 4);
   idle_thread.ctx.srr0 = (uint32_t)idle_loop;
   idle_thread.prio     = GK_PRIO_IDLE;
   idle_thread.stack_lo = idle_stack;
   idle_stack[0]        = STACK_CANARY;
   rq_push(&idle_thread, 0);
}

struct gk_thread *gk_sched_main(void)
{
   return &main_thread;
}

gk_thread_t *gk_thread_create(gk_thread_fn fn, void *arg,
      void *stack, size_t stack_size, int prio)
{
   struct gk_thread *t;
   uint32_t level;

   free_zombies();
   if (prio < GK_PRIO_MIN)
      prio = GK_PRIO_MIN;
   if (prio > GK_PRIO_MAX)
      prio = GK_PRIO_MAX;
   if (stack_size < 4096)
      stack_size = 4096;
   if (!(t = (struct gk_thread*)memalign(32, sizeof(*t))))
      return NULL;
   memset(t, 0, sizeof(*t));
   if (!stack)
   {
      if (!(stack = memalign(32, stack_size)))
      {
         free(t);
         return NULL;
      }
      t->stack = stack;
   }
   t->stack_lo  = (uint32_t*)stack;
   *t->stack_lo = STACK_CANARY;
   t->fn        = fn;
   t->arg       = arg;
   t->prio      = (uint8_t)prio;
   _REENT_INIT_PTR(&t->reent);
   ctx_setup(t, (uint32_t*)((uint8_t*)stack + stack_size));

   level = gk_irq_disable();
   t->all_next = all_threads;
   all_threads = t;
   rq_push(t, 0);
   maybe_preempt();
   gk_irq_restore(level);
   return t;
}

void gk_thread_exit(void *ret)
{
   struct gk_thread *self = gk_cur;
   gk_tls_run_dtors(self);
   _reclaim_reent(&self->reent);
   gk_irq_disable();
   self->ret    = ret;
   self->exited = 1;
   self->state  = GK_T_DEAD;
   if (gk_fp_owner == self)
      gk_fp_owner = NULL;
   if (self->joiner)
      rq_push(self->joiner, 0);
   else if (self->detached)
   {
      self->tnext = zombies;
      zombies     = self;
   }
   gk_syscall_resched();
   for (;;)
      ;
}

static void thread_entry(struct gk_thread *t)
{
   gk_thread_exit(t->fn(t->arg));
}

static void unlink_all(struct gk_thread *t)
{
   struct gk_thread **pp = &all_threads;
   while (*pp && *pp != t)
      pp = &(*pp)->all_next;
   if (*pp)
      *pp = t->all_next;
}

static void destroy(struct gk_thread *t)
{
   uint32_t level = gk_irq_disable();
   unlink_all(t);
   gk_irq_restore(level);
   if (t->stack)
      free(t->stack);
   free(t);
}

static void free_zombies(void)
{
   struct gk_thread *list;
   uint32_t level = gk_irq_disable();
   list    = zombies;
   zombies = NULL;
   gk_irq_restore(level);
   while (list)
   {
      struct gk_thread *n = list->tnext;
      destroy(list);
      list = n;
   }
}

void *gk_thread_join(gk_thread_t *t)
{
   void *ret;
   uint32_t level = gk_irq_disable();
   if (!t->exited)
   {
      t->joiner = gk_cur;
      gk_cur->state = GK_T_BLOCKED;
      gk_syscall_resched();
   }
   gk_irq_restore(level);
   ret = t->ret;
   destroy(t);
   return ret;
}

void gk_thread_detach(gk_thread_t *t)
{
   uint32_t level = gk_irq_disable();
   if (t->exited)
   {
      t->tnext = zombies;
      zombies  = t;
   }
   else
      t->detached = 1;
   gk_irq_restore(level);
}

gk_thread_t *gk_thread_self(void)
{
   return gk_cur;
}

void gk_thread_yield(void)
{
   uint32_t level = gk_irq_disable();
   yield_req = 1;
   gk_syscall_resched();
   gk_irq_restore(level);
}

int gk_thread_get_prio(gk_thread_t *t)
{
   return t->prio;
}

void gk_thread_set_prio(gk_thread_t *t, int prio)
{
   uint32_t level;
   if (prio < GK_PRIO_MIN)
      prio = GK_PRIO_MIN;
   if (prio > GK_PRIO_MAX)
      prio = GK_PRIO_MAX;
   level = gk_irq_disable();
   if (t->state == GK_T_READY)
   {
      rq_remove(t);
      t->prio = (uint8_t)prio;
      rq_push(t, 0);
   }
   else if (t->state == GK_T_BLOCKED && t->wait_addr)
   {
      volatile uint32_t *addr = t->wait_addr;
      fq_remove(t);
      t->prio = (uint8_t)prio;
      fq_insert(t, addr);
   }
   else
      t->prio = (uint8_t)prio;
   if (t == gk_cur || t->state == GK_T_READY)
   {
      if (ready_top() > (int)gk_cur->prio)
         gk_syscall_resched();
   }
   gk_irq_restore(level);
}

void gk_sleep_ticks(uint64_t ticks)
{
   static volatile uint32_t never;
   uint32_t level = gk_irq_disable();
   block_on(&never, ticks ? ticks : 1);
   gk_irq_restore(level);
}

void gk_sleep_us(uint64_t us)
{
   gk_sleep_ticks(GK_US_TO_TICKS(us));
}

/* ---- futex ---- */

int gk_futex_wait(volatile uint32_t *addr, uint32_t expected,
      uint64_t timeout)
{
   int ret;
   uint32_t level = gk_irq_disable();
   if (*addr != expected)
      ret = GK_EAGAIN;
   else
      ret = block_on(addr, timeout);
   gk_irq_restore(level);
   return ret;
}

static int wake_n(volatile uint32_t *addr, int count)
{
   int n = 0;
   struct gk_thread *t;
   while (n < count && (t = fq_first(addr)))
   {
      wake_one(t, GK_OK);
      n++;
   }
   return n;
}

int gk_futex_wake(volatile uint32_t *addr, int count)
{
   int n;
   uint32_t level = gk_irq_disable();
   n = wake_n(addr, count);
   if (n)
      maybe_preempt();
   gk_irq_restore(level);
   return n;
}

/* ---- mutex: word = owner | waiters bit ---- */

#define MX_WAITERS 1u

static int mutex_lock_t(gk_mutex_t *m, uint64_t timeout)
{
   int ret = GK_OK;
   uint32_t self  = (uint32_t)gk_cur;
   uint32_t level = gk_irq_disable();
   if (!m->word)
      m->word = self;
   else if ((m->word & ~MX_WAITERS) == self)
      gk_panic("mutex %p relocked by its owner", (void*)m);
   else
   {
      m->word |= MX_WAITERS;
      /* Unlock hands the word over, so a normal wake means we own it. */
      if ((ret = block_on(&m->word, timeout)) == GK_ETIMEDOUT
            && !fq_first(&m->word))
         m->word &= ~MX_WAITERS;
   }
   gk_irq_restore(level);
   return ret;
}

void gk_mutex_lock(gk_mutex_t *m)
{
   mutex_lock_t(m, GK_WAIT_FOREVER);
}

int gk_mutex_timedlock(gk_mutex_t *m, uint64_t timeout)
{
   return mutex_lock_t(m, timeout);
}

int gk_mutex_trylock(gk_mutex_t *m)
{
   int ret = 1;
   uint32_t level = gk_irq_disable();
   if (!m->word)
   {
      m->word = (uint32_t)gk_cur;
      ret     = 0;
   }
   gk_irq_restore(level);
   return ret;
}

/* Interrupts off.  Returns the new owner if one was woken. */
static struct gk_thread *mutex_release(gk_mutex_t *m)
{
   struct gk_thread *next;
   if (!(m->word & MX_WAITERS) || !(next = fq_first(&m->word)))
   {
      m->word = 0;
      return NULL;
   }
   wake_one(next, GK_OK);
   m->word = (uint32_t)next | (fq_first(&m->word) ? MX_WAITERS : 0);
   return next;
}

void gk_mutex_unlock(gk_mutex_t *m)
{
   uint32_t level = gk_irq_disable();
   if (mutex_release(m))
      maybe_preempt();
   gk_irq_restore(level);
}

void gk_rmutex_lock(gk_rmutex_t *m)
{
   if ((m->m.word & ~MX_WAITERS) == (uint32_t)gk_cur)
   {
      m->depth++;
      return;
   }
   gk_mutex_lock(&m->m);
   m->depth = 1;
}

int gk_rmutex_trylock(gk_rmutex_t *m)
{
   if ((m->m.word & ~MX_WAITERS) == (uint32_t)gk_cur)
   {
      m->depth++;
      return 0;
   }
   if (gk_mutex_trylock(&m->m))
      return 1;
   m->depth = 1;
   return 0;
}

void gk_rmutex_unlock(gk_rmutex_t *m)
{
   if (--m->depth == 0)
      gk_mutex_unlock(&m->m);
}

/* ---- condition variables ---- */

int gk_cond_wait(gk_cond_t *c, gk_mutex_t *m, uint64_t timeout)
{
   int ret;
   uint32_t level = gk_irq_disable();
   mutex_release(m);
   ret = block_on(&c->seq, timeout);
   gk_irq_restore(level);
   gk_mutex_lock(m);
   return ret == GK_ETIMEDOUT ? GK_ETIMEDOUT : GK_OK;
}

int gk_cond_wait_r(gk_cond_t *c, gk_rmutex_t *m, uint64_t timeout)
{
   int ret;
   uint32_t depth = m->depth;
   uint32_t level = gk_irq_disable();
   m->depth = 0;
   mutex_release(&m->m);
   ret = block_on(&c->seq, timeout);
   gk_irq_restore(level);
   gk_mutex_lock(&m->m);
   m->depth = depth;
   return ret == GK_ETIMEDOUT ? GK_ETIMEDOUT : GK_OK;
}

void gk_cond_signal(gk_cond_t *c)
{
   uint32_t level = gk_irq_disable();
   c->seq++;
   if (wake_n(&c->seq, 1))
      maybe_preempt();
   gk_irq_restore(level);
}

void gk_cond_broadcast(gk_cond_t *c)
{
   uint32_t level = gk_irq_disable();
   c->seq++;
   if (wake_n(&c->seq, 0x7fffffff))
      maybe_preempt();
   gk_irq_restore(level);
}

/* ---- semaphores: a post with sleepers hands its unit straight over ---- */

void gk_sem_init(gk_sem_t *s, uint32_t count)
{
   s->count = count;
}

int gk_sem_wait(gk_sem_t *s, uint64_t timeout)
{
   int ret = GK_OK;
   uint32_t level = gk_irq_disable();
   if (s->count)
      s->count--;
   else
      ret = block_on(&s->count, timeout);
   gk_irq_restore(level);
   return ret;
}

void gk_sem_post(gk_sem_t *s)
{
   uint32_t level = gk_irq_disable();
   if (wake_n(&s->count, 1))
      maybe_preempt();
   else
      s->count++;
   gk_irq_restore(level);
}
