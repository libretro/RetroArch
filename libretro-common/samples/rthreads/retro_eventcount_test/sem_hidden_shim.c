/* The semaphore backend sleeps on a Mach semaphore on Darwin, which
 * ThreadSanitizer does not intercept: it sees no order through one.
 * The POSIX semaphore this host builds that backend over is one it
 * does intercept, which would hide a race only Darwin shows.  So the
 * backend's three calls come here and on to the C library past the
 * interceptors. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

int hidden_sem_wait(sem_t *sem);
int hidden_sem_post(sem_t *sem);
int hidden_sem_timedwait(sem_t *sem, const struct timespec *ts);

static int (*real_wait)(sem_t*);
static int (*real_post)(sem_t*);
static int (*real_timedwait)(sem_t*, const struct timespec*);

/* Before main, so no thread reads the pointers while they are set. */
__attribute__((constructor)) static void hidden_sem_init(void)
{
   void *libc = dlopen("libc.so.6", RTLD_NOW | RTLD_NOLOAD);
   if (!libc)
      abort();
   real_wait      = (int (*)(sem_t*))dlsym(libc, "sem_wait");
   real_post      = (int (*)(sem_t*))dlsym(libc, "sem_post");
   real_timedwait = (int (*)(sem_t*, const struct timespec*))
      dlsym(libc, "sem_timedwait");
   if (!real_wait || !real_post || !real_timedwait)
      abort();
}

int hidden_sem_wait(sem_t *sem) { return real_wait(sem); }
int hidden_sem_post(sem_t *sem) { return real_post(sem); }
int hidden_sem_timedwait(sem_t *sem, const struct timespec *ts)
{
   return real_timedwait(sem, ts);
}
