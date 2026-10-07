/* Wii: the IPC mailbox to IOS.
 *
 * The processor posts a request's physical address in PPCMSG and sets
 * X1; IOS acknowledges with Y2 and later posts the same address in
 * ARMMSG with Y1 once it has written the result back.  One request is
 * in the mailbox at a time; the rest wait in a queue the interrupt
 * handler drains on each acknowledgement. */

#include <string.h>

#include <gekko/ios.h>

#include "../kernel/kernel.h"

#define HW_IPC_PPCMSG   0xcd000000u
#define HW_IPC_PPCCTRL  0xcd000004u
#define HW_IPC_ARMMSG   0xcd000008u
#define HW_PPCIRQFLAG   0xcd000030u
#define HW_PPCIRQMASK   0xcd000034u

#define CTRL_X1   0x01u
#define CTRL_Y2   0x02u
#define CTRL_Y1   0x04u
#define CTRL_X2   0x08u
#define CTRL_IRQ  0x30u   /* reply and acknowledge interrupts */
#define IRQ_IPC   (1u << 30)

#define LOMEM_IOS_VERSION 0x80003140u

enum
{
   CMD_OPEN = 1,
   CMD_CLOSE,
   CMD_READ,
   CMD_WRITE,
   CMD_SEEK,
   CMD_IOCTL,
   CMD_IOCTLV
};

#define MAX_VECS 8
#define POOL     32

struct ios_req
{
   /* The first cache line is IOS's while the request is out. */
   uint32_t cmd;
   int32_t  result;
   int32_t  fd;
   uint32_t arg[5];
   /* Read by IOS: a path, or the vector table (address, length). */
   uint32_t aux[MAX_VECS * 2];
   /* Ours. */
   void              *out[MAX_VECS];
   uint32_t           out_len[MAX_VECS];
   gk_ios_cb          cb;
   void              *cb_data;
   struct ios_req    *next;
   volatile uint32_t  done;
   uint32_t           n_out;
   uint32_t           busy;         /* IOS has it */
   uint32_t           pad[2];
};

typedef char assert_req_lines[(sizeof(struct ios_req) & 31) == 0 ? 1 : -1];

static struct ios_req  pool[POOL] __attribute__((aligned(32)));
static struct ios_req *free_list;
static gk_sem_t        free_count;
static struct ios_req *queue_head, *queue_tail;
static uint32_t        mailbox_busy;
static uint32_t        started;
static volatile uint32_t acks;      /* acknowledgements seen */

static void post(struct ios_req *r)
{
   GK_REG32(HW_IPC_PPCMSG)  = GK_PHYS(r);
   GK_REG32(HW_IPC_PPCCTRL) = CTRL_IRQ | CTRL_X1;
   mailbox_busy = 1;
}

/* Interrupts off. */
static void submit(struct ios_req *r)
{
   gk_dcache_flush(r, offsetof(struct ios_req, out));
   r->next = NULL;
   r->busy = 1;
   if (mailbox_busy)
   {
      if (queue_tail)
         queue_tail->next = r;
      else
         queue_head = r;
      queue_tail = r;
   }
   else
      post(r);
}

static void release(struct ios_req *r)
{
   uint32_t level = gk_irq_disable();
   r->next   = free_list;
   free_list = r;
   gk_irq_restore(level);
   gk_sem_post(&free_count);
}

static void complete(struct ios_req *r)
{
   uint32_t i;
   gk_dcache_invalidate(r, 32);
   for (i = 0; i < r->n_out; i++)
      gk_dcache_invalidate(r->out[i], r->out_len[i]);
   if (r->cb)
   {
      gk_ios_cb cb   = r->cb;
      void     *data = r->cb_data;
      int32_t   res  = r->result;
      release(r);
      cb(res, data);
   }
   else
   {
      r->done = 1;
      gk_futex_wake(&r->done, 1);
   }
}

static void ipc_irq(enum gk_irq irq, void *data)
{
   uint32_t ctrl = GK_REG32(HW_IPC_PPCCTRL);
   (void)irq;
   (void)data;
   if (ctrl & CTRL_Y2)
   {
      GK_REG32(HW_IPC_PPCCTRL) = CTRL_IRQ | CTRL_Y2;
      GK_REG32(HW_PPCIRQFLAG)  = IRQ_IPC;
      mailbox_busy = 0;
      acks++;
      if (queue_head)
      {
         struct ios_req *r = queue_head;
         if (!(queue_head = r->next))
            queue_tail = NULL;
         post(r);
      }
   }
   if (ctrl & CTRL_Y1)
   {
      uint32_t msg = GK_REG32(HW_IPC_ARMMSG);
      GK_REG32(HW_IPC_PPCCTRL) = CTRL_IRQ | CTRL_Y1;
      GK_REG32(HW_PPCIRQFLAG)  = IRQ_IPC;
      /* Replies to the program before this one are not ours. */
      if (msg)
      {
         uint32_t off = (uint32_t)GK_CACHED(msg) - (uint32_t)pool;
         uint32_t n   = off / sizeof(pool[0]);
         if (off < sizeof(pool) && !(off % sizeof(pool[0])) && pool[n].busy)
         {
            pool[n].busy = 0;
            complete(&pool[n]);
         }
      }
      GK_REG32(HW_IPC_PPCCTRL) = CTRL_IRQ | CTRL_X2;
   }
   if (!(ctrl & (CTRL_Y1 | CTRL_Y2)))
      GK_REG32(HW_PPCIRQFLAG) = IRQ_IPC;
}

static void start(void)
{
   unsigned i;
   uint32_t level = gk_irq_disable();
   if (started)
   {
      gk_irq_restore(level);
      return;
   }
   started = 1;
   for (i = 0; i < POOL; i++)
   {
      pool[i].next = free_list;
      free_list    = &pool[i];
   }
   gk_sem_init(&free_count, POOL);
   GK_REG32(HW_PPCIRQFLAG)  = IRQ_IPC;
   GK_REG32(HW_PPCIRQMASK)  = IRQ_IPC;
   GK_REG32(HW_IPC_PPCCTRL) = CTRL_IRQ;
   gk_irq_set(GK_IRQ_IPC, ipc_irq, NULL);
   gk_irq_restore(level);
}

/* NULL only from an interrupt handler with every request out. */
static struct ios_req *get(uint32_t cmd, int32_t fd)
{
   struct ios_req *r;
   uint32_t level;
   if (!started)
      start();
   if (gk_sem_wait(&free_count, gk_in_exception ? 0 : GK_WAIT_FOREVER))
      return NULL;
   level = gk_irq_disable();
   r         = free_list;
   free_list = r->next;
   gk_irq_restore(level);
   memset(r, 0, offsetof(struct ios_req, pad));
   r->cmd = cmd;
   r->fd  = fd;
   return r;
}

static int32_t run(struct ios_req *r, gk_ios_cb cb, void *data)
{
   int32_t ret = 0;
   uint32_t level;
   if (!r)
      return GK_IOS_ENOMEM;
   r->cb      = cb;
   r->cb_data = data;
   level = gk_irq_disable();
   submit(r);
   gk_irq_restore(level);
   if (cb)
      return 0;
   while (!r->done)
      gk_futex_wait(&r->done, 0, GK_WAIT_FOREVER);
   ret = r->result;
   release(r);
   return ret;
}

static void out_buf(struct ios_req *r, void *p, uint32_t len)
{
   if (p && len && r->n_out < MAX_VECS)
   {
      /* Nothing of ours may sit dirty over what IOS writes. */
      gk_dcache_flush(p, len);
      r->out[r->n_out]     = p;
      r->out_len[r->n_out] = len;
      r->n_out++;
   }
}

int32_t gk_ios_open(const char *path, int mode)
{
   struct ios_req *r;
   size_t len = strlen(path);
   if (len >= sizeof(r->aux))
      return GK_IOS_ENOENT;
   if (!(r = get(CMD_OPEN, 0)))
      return GK_IOS_ENOMEM;
   memcpy(r->aux, path, len + 1);
   r->arg[0] = GK_PHYS(r->aux);
   r->arg[1] = (uint32_t)mode;
   return run(r, NULL, NULL);
}

int32_t gk_ios_close(int32_t fd)
{
   return run(get(CMD_CLOSE, fd), NULL, NULL);
}

int32_t gk_ios_read(int32_t fd, void *buf, uint32_t len)
{
   struct ios_req *r = get(CMD_READ, fd);
   if (!r)
      return GK_IOS_ENOMEM;
   r->arg[0] = GK_PHYS(buf);
   r->arg[1] = len;
   out_buf(r, buf, len);
   return run(r, NULL, NULL);
}

int32_t gk_ios_write(int32_t fd, const void *buf, uint32_t len)
{
   struct ios_req *r = get(CMD_WRITE, fd);
   if (!r)
      return GK_IOS_ENOMEM;
   gk_dcache_flush(buf, len);
   r->arg[0] = GK_PHYS(buf);
   r->arg[1] = len;
   return run(r, NULL, NULL);
}

int32_t gk_ios_seek(int32_t fd, int32_t offset, int whence)
{
   struct ios_req *r = get(CMD_SEEK, fd);
   if (!r)
      return GK_IOS_ENOMEM;
   r->arg[0] = (uint32_t)offset;
   r->arg[1] = (uint32_t)whence;
   return run(r, NULL, NULL);
}

static struct ios_req *ioctl_req(int32_t fd, uint32_t cmd, const void *in,
      uint32_t in_len, void *out, uint32_t out_len)
{
   struct ios_req *r = get(CMD_IOCTL, fd);
   if (!r)
      return NULL;
   if (in && in_len)
      gk_dcache_flush(in, in_len);
   out_buf(r, out, out_len);
   r->arg[0] = cmd;
   r->arg[1] = in  ? GK_PHYS(in)  : 0;
   r->arg[2] = in_len;
   r->arg[3] = out ? GK_PHYS(out) : 0;
   r->arg[4] = out_len;
   return r;
}

static struct ios_req *ioctlv_req(int32_t fd, uint32_t cmd, uint32_t n_in,
      uint32_t n_io, const gk_ios_vec_t *vec)
{
   uint32_t i;
   struct ios_req *r;
   if (n_in + n_io > MAX_VECS)
      return NULL;
   if (!(r = get(CMD_IOCTLV, fd)))
      return NULL;
   for (i = 0; i < n_in + n_io; i++)
   {
      r->aux[i * 2]     = vec[i].data ? GK_PHYS(vec[i].data) : 0;
      r->aux[i * 2 + 1] = vec[i].len;
      if (i < n_in)
      {
         if (vec[i].data && vec[i].len)
            gk_dcache_flush(vec[i].data, vec[i].len);
      }
      else
         out_buf(r, vec[i].data, vec[i].len);
   }
   r->arg[0] = cmd;
   r->arg[1] = n_in;
   r->arg[2] = n_io;
   r->arg[3] = GK_PHYS(r->aux);
   return r;
}

int32_t gk_ios_ioctl(int32_t fd, uint32_t cmd, const void *in,
      uint32_t in_len, void *out, uint32_t out_len)
{
   return run(ioctl_req(fd, cmd, in, in_len, out, out_len), NULL, NULL);
}

int32_t gk_ios_ioctlv(int32_t fd, uint32_t cmd, uint32_t n_in,
      uint32_t n_io, const gk_ios_vec_t *vec)
{
   struct ios_req *r = ioctlv_req(fd, cmd, n_in, n_io, vec);
   return r ? run(r, NULL, NULL) : GK_IOS_ENOMEM;
}

int32_t gk_ios_ioctl_async(int32_t fd, uint32_t cmd, const void *in,
      uint32_t in_len, void *out, uint32_t out_len,
      gk_ios_cb cb, void *data)
{
   return run(ioctl_req(fd, cmd, in, in_len, out, out_len), cb, data);
}

int32_t gk_ios_ioctlv_async(int32_t fd, uint32_t cmd, uint32_t n_in,
      uint32_t n_io, const gk_ios_vec_t *vec, gk_ios_cb cb, void *data)
{
   struct ios_req *r = ioctlv_req(fd, cmd, n_in, n_io, vec);
   return r ? run(r, cb, data) : GK_IOS_ENOMEM;
}

uint32_t gk_ios_version(void)
{
   return *(volatile uint32_t*)LOMEM_IOS_VERSION;
}

/* A launch that works is never answered: an answer is a refusal. */
static volatile int32_t launch_refused;

static void launch_done(int32_t result, void *data)
{
   (void)data;
   launch_refused = result < 0 ? result : -1;
}

/* An ES launch of an IOS (es.c, vec as ES_LaunchTitle takes it): the
 * request is acknowledged but never answered.  IOS writes the new
 * version to low memory as it goes, and the new one acknowledges once
 * it takes requests; then everything of the old one's is forgotten. */
int gk_ipc_launch_ios(int32_t es_fd, const gk_ios_vec_t *vec,
      uint32_t major)
{
   uint64_t end;
   uint32_t level, seen, i;
   volatile uint32_t *version = (volatile uint32_t*)LOMEM_IOS_VERSION;
   uint32_t old               = *version;
   *version = 0;
   gk_dcache_flush((void*)LOMEM_IOS_VERSION, 4);
   launch_refused = 0;
   seen           = acks;
   if (gk_ios_ioctlv_async(es_fd, 0x08, 2, 0, vec, launch_done, NULL) < 0)
      return -1;
   end = gk_ticks() + GK_US_TO_TICKS(10000000);
   for (;;)
   {
      gk_dcache_invalidate((void*)LOMEM_IOS_VERSION, 4);
      if ((*version >> 16) == major)
         break;
      if (launch_refused || gk_ticks() > end)
      {
         /* Still the old IOS */
         *version = old;
         gk_dcache_flush((void*)LOMEM_IOS_VERSION, 4);
         return -1;
      }
      gk_sleep_us(1000);
   }
   /* Two acknowledgements: the old IOS's for the request, the new
    * one's as it comes up; the new one starts with the mailbox's
    * interrupts off. */
   end = gk_ticks() + GK_US_TO_TICKS(2000000);
   while (acks - seen < 2 && gk_ticks() < end)
   {
      level = gk_irq_disable();
      GK_REG32(HW_PPCIRQMASK)  = IRQ_IPC;
      GK_REG32(HW_IPC_PPCCTRL)  = CTRL_IRQ;
      gk_irq_restore(level);
      gk_sleep_us(1000);
   }
   level = gk_irq_disable();
   for (i = 0; i < POOL; i++)
      if (pool[i].busy)
      {
         pool[i].busy = 0;
         pool[i].next = free_list;
         free_list    = &pool[i];
         gk_sem_post(&free_count);
      }
   queue_head   = NULL;
   queue_tail   = NULL;
   mailbox_busy = 0;
   gk_irq_restore(level);
   return 0;
}
