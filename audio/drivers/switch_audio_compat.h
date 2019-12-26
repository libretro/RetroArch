#ifndef _SWITCH_AUDIO_COMPAT_H
#define _SWITCH_AUDIO_COMPAT_H

#ifdef HAVE_LIBNX
#include <switch.h>
#else
#include <libtransistor/nx.h>
#endif

#ifdef HAVE_LIBNX

/* libnx definitions */

/* threading */
typedef Mutex compat_mutex;
typedef Thread compat_thread;
typedef CondVar compat_condvar;

#define compat_thread_create(thread, func, data, stack_size, prio, cpu) \
   threadCreate(thread, func, data, NULL, stack_size, prio, cpu)
#define compat_thread_start(thread) \
   threadStart(thread)
#define compat_thread_join(thread) \
   threadWaitForExit(thread)
#define compat_thread_close(thread) \
   threadClose(thread)
#define compat_mutex_create(mutex) \
   mutexInit(mutex)
#define compat_mutex_lock(mutex) \
   mutexLock(mutex)
#define compat_mutex_unlock(mutex) \
   mutexUnlock(mutex)
#define compat_condvar_create(condvar) \
   condvarInit(condvar)
#define compat_condvar_wait(condvar, mutex) \
   condvarWait(condvar, mutex)
#define compat_condvar_wake_all(condvar) \
   condvarWakeAll(condvar)

/* audio */
typedef AudioOutBuffer compat_audio_out_buffer;
#define switch_audio_ipc_init audoutInitialize
#define switch_audio_ipc_finalize audoutExit
#define switch_audio_ipc_output_get_released_buffer(a, b) audoutGetReleasedAudioOutBuffer(&a->current_buffer, &b)
#define switch_audio_ipc_output_append_buffer(a, b) audoutAppendAudioOutBuffer(b)
#define switch_audio_ipc_output_stop(a) audoutStopAudioOut()
#define switch_audio_ipc_output_start(a) audoutStartAudioOut()

#else

/* libtransistor definitions */

typedef result_t Result;
#define R_FAILED(r) ((r) != RESULT_OK)

/* threading */
typedef trn_mutex_t compat_mutex;
typedef trn_thread_t compat_thread;
typedef trn_condvar_t compat_condvar;

#define compat_thread_create(thread, func, data, stack_size, prio, cpu) \
   trn_thread_create(thread, func, data, prio, cpu, stack_size, NULL)
#define compat_thread_start(thread) \
   trn_thread_start(thread)
#define compat_thread_join(thread) \
   trn_thread_join(thread, -1)
#define compat_thread_close(thread) \
   trn_thread_destroy(thread)
#define compat_mutex_create(mutex) \
   trn_mutex_create(mutex)
#define compat_mutex_lock(mutex) \
   trn_mutex_lock(mutex)
#define compat_mutex_unlock(mutex) \
   trn_mutex_unlock(mutex)
#define compat_condvar_create(condvar) \
   trn_condvar_create(condvar)
#define compat_condvar_wait(condvar, mutex) \
   trn_condvar_wait(condvar, mutex, -1)
#define compat_condvar_wake_all(condvar) \
   trn_condvar_signal(condvar, -1)

/* audio */
typedef audio_output_buffer_t compat_audio_out_buffer;
#define switch_audio_ipc_init audio_ipc_init
#define switch_audio_ipc_finalize audio_ipc_finalize
#define switch_audio_ipc_output_get_released_buffer(a, b) audio_ipc_output_get_released_buffer(&a->output, &b, &a->current_buffer)
#define switch_audio_ipc_output_append_buffer(a, b) audio_ipc_output_append_buffer(&a->output, b)
#define switch_audio_ipc_output_stop(a) audio_ipc_output_stop(&a->output)
#define switch_audio_ipc_output_start(a) audio_ipc_output_start(&a->output)

#endif

#endif
