/* Compile-only stand-in for the bionic-only pthread entry points the
 * Android arms of libretro-common name, so the compile matrix can pass
 * them through a host toolchain. Never linked. */
#ifndef BIONIC_PTHREAD_STUB_H
#define BIONIC_PTHREAD_STUB_H
#include <sys/types.h>
#include <pthread.h>
pid_t pthread_gettid_np(pthread_t t);
/* bionic declares gettid unconditionally; glibc only under _GNU_SOURCE,
 * which this -include lands ahead of. */
pid_t gettid(void);
#endif
