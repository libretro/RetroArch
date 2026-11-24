/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_dispatch.h"
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>

#if defined(HAVE_THREADS) && RETRO_ATOMIC_LOCK_FREE && !defined(__WINRT__) && !defined(_XBOX) \
   && (defined(_WIN32) || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) \
       || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#endif

struct record_pyrowave_dispatch
{
   record_pyrowave_connection_t *connection;
   record_pyrowave_video_t decision_video;
   record_pyrowave_pcm_t *decision_pcm;
   rpyrowave_stream_config_t config;
   uint64_t generation;
   struct sockaddr_storage bound_address;
   size_t bound_bytes;
   unsigned decision_reason;
   retro_atomic_int_t state, reason, cancel, finish, abandon, decision, configured, finished;
   bool sender, listen;
#if defined(_WIN32)
   HANDLE wake, thread;
   WSAEVENT network_event;
   SOCKET registered_socket;
#else
   int wake_read, wake_write;
#endif
};

void record_pyrowave_dispatch_notify(record_pyrowave_dispatch_t *j)
{
   if (!j) return;
#if defined(_WIN32)
   SetEvent(j->wake);
#else
   {
      const unsigned char byte = 1;
      ssize_t result;
      do
         result = write(j->wake_write, &byte, 1);
      while (result < 0 && errno == EINTR);
      (void)result; /* A full nonblocking pipe already carries a wake. */
   }
#endif
}

#if defined(_WIN32)
static void record_pyrowave_dispatch_disarm(record_pyrowave_dispatch_t *j)
{
   if (j->registered_socket != INVALID_SOCKET)
   {
      WSAEventSelect(j->registered_socket, j->network_event, 0);
      j->registered_socket = INVALID_SOCKET;
   }
   WSAResetEvent(j->network_event);
}
#else
static void record_pyrowave_dispatch_disarm(record_pyrowave_dispatch_t *j)
{
   (void)j;
}
#endif

static int record_pyrowave_dispatch_wait(record_pyrowave_dispatch_t *j, unsigned *readiness)
{
   unsigned interest = record_pyrowave_connection_interest(j->connection);
   uintptr_t handle = record_pyrowave_connection_handle(j->connection);
   *readiness = 0;
#if defined(_WIN32)
   {
      HANDLE handles[2];
      DWORD result, count = 1;
      long mask = 0;
      handles[0] = j->wake;
      if (interest)
      {
         mask = FD_CLOSE;
         if (interest & RPYROWAVE_ENDPOINT_READABLE) mask |= record_pyrowave_connection_listening(j->connection) ? FD_ACCEPT : FD_READ;
         if (interest & RPYROWAVE_ENDPOINT_WRITABLE) mask |= FD_WRITE | (!j->listen ? FD_CONNECT : 0);
         if (interest & RPYROWAVE_ENDPOINT_EXCEPTION) mask |= FD_OOB;
         if (WSAEventSelect((SOCKET)handle, j->network_event, mask)) return 0;
         j->registered_socket = (SOCKET)handle;
         handles[1] = j->network_event; count = 2;
         {
            fd_set reads, writes, errors;
            struct timeval zero;
            int ready;
            FD_ZERO(&reads); FD_ZERO(&writes); FD_ZERO(&errors);
            if (interest & RPYROWAVE_ENDPOINT_READABLE) FD_SET((SOCKET)handle, &reads);
            if (interest & RPYROWAVE_ENDPOINT_WRITABLE) FD_SET((SOCKET)handle, &writes);
            FD_SET((SOCKET)handle, &errors);
            zero.tv_sec = zero.tv_usec = 0;
            /* Recheck level readiness after arming: re-registration can
             * otherwise lose a completed connect's one-shot notification. */
            ready = select(0, &reads, &writes, &errors, &zero);
            if (ready == SOCKET_ERROR) return 0;
            if (ready)
            {
               if (FD_ISSET((SOCKET)handle, &reads)) *readiness |= RPYROWAVE_ENDPOINT_READABLE;
               if (FD_ISSET((SOCKET)handle, &writes)) *readiness |= RPYROWAVE_ENDPOINT_WRITABLE;
               if (FD_ISSET((SOCKET)handle, &errors)) *readiness |= RPYROWAVE_ENDPOINT_EXCEPTION;
               return 1;
            }
         }
      }
      result = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
      if (result == WAIT_OBJECT_0) return 1;
      if (count == 2 && result == WAIT_OBJECT_0 + 1)
      {
         WSANETWORKEVENTS events;
         unsigned i;
         if (WSAEnumNetworkEvents((SOCKET)handle, j->network_event, &events)) return 0;
         if (events.lNetworkEvents & (FD_READ | FD_ACCEPT | FD_CLOSE)) *readiness |= RPYROWAVE_ENDPOINT_READABLE;
         if (events.lNetworkEvents & (FD_WRITE | FD_CONNECT)) *readiness |= RPYROWAVE_ENDPOINT_WRITABLE;
         if (events.lNetworkEvents & FD_OOB) *readiness |= RPYROWAVE_ENDPOINT_EXCEPTION;
         for (i = 0; i < FD_MAX_EVENTS; i++)
            if ((events.lNetworkEvents & (1L << i)) && events.iErrorCode[i]) *readiness |= RPYROWAVE_ENDPOINT_EXCEPTION;
         return 1;
      }
      return 0;
   }
#else
   {
      struct pollfd descriptors[2];
      unsigned char wakes[128];
      int result;
      nfds_t count = 1;
      memset(descriptors, 0, sizeof(descriptors));
      descriptors[0].fd = j->wake_read; descriptors[0].events = POLLIN;
      if (interest)
      {
         if (handle > INT_MAX) return 0;
         descriptors[1].fd = (int)handle;
         if (interest & RPYROWAVE_ENDPOINT_READABLE) descriptors[1].events |= POLLIN;
         if (interest & RPYROWAVE_ENDPOINT_WRITABLE) descriptors[1].events |= POLLOUT;
         if (interest & RPYROWAVE_ENDPOINT_EXCEPTION) descriptors[1].events |= POLLPRI;
         count = 2;
      }
      result = poll(descriptors, count, -1);
      if (result < 0) return errno == EINTR;
      if (descriptors[0].revents & POLLIN)
      {
         ssize_t consumed = read(j->wake_read, wakes, sizeof(wakes));
         (void)consumed;
      }
      if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return 0;
      if (count == 2)
      {
         if (descriptors[1].revents & (POLLIN | POLLHUP)) *readiness |= RPYROWAVE_ENDPOINT_READABLE;
         if (descriptors[1].revents & POLLOUT) *readiness |= RPYROWAVE_ENDPOINT_WRITABLE;
         if (descriptors[1].revents & (POLLPRI | POLLERR | POLLNVAL)) *readiness |= RPYROWAVE_ENDPOINT_EXCEPTION;
      }
      return 1;
   }
#endif
}

static void record_pyrowave_dispatch_run(record_pyrowave_dispatch_t *j)
{
   unsigned readiness = 0, terminal = RECORD_PYROWAVE_DISPATCH_DONE;
   bool finishing = false, failed = false;
   for (;;)
   {
      record_pyrowave_session_t *session = record_pyrowave_connection_session(j->connection);
      record_pyrowave_session_status_t status;
      int result;
      unsigned state;
      record_pyrowave_dispatch_disarm(j);
      if (retro_atomic_load_acquire_int(&j->cancel)) record_pyrowave_connection_abort(j->connection);
      if (retro_atomic_load_acquire_int(&j->abandon)) record_pyrowave_session_abandon_video(session);
      if (retro_atomic_load_acquire_int(&j->decision) == 2)
      {
         bool accepted = j->decision_reason ? record_pyrowave_session_reject(session, j->decision_reason)
            : record_pyrowave_session_accept(session, &j->decision_video, j->decision_pcm);
         retro_atomic_store_release_int(&j->decision, 3);
         if (!accepted && !retro_atomic_load_acquire_int(&j->cancel))
         {
            failed = true; record_pyrowave_connection_abort(j->connection);
         }
      }
      result = record_pyrowave_connection_step(j->connection, readiness);
      if (result < 0) failed = true;
      record_pyrowave_session_status(session, &status);
      if (!finishing && j->sender && retro_atomic_load_acquire_int(&j->finish) && status.state == RECORD_PYROWAVE_ACTIVE)
      {
         finishing = record_pyrowave_session_finish(session);
         if (!finishing || record_pyrowave_connection_step(j->connection, 0) < 0) failed = true;
         record_pyrowave_session_status(session, &status);
      }
      retro_atomic_store_release_int(&j->reason, (int)status.reason);
      if (!retro_atomic_load_acquire_int(&j->configured) && status.state == RECORD_PYROWAVE_CONFIGURING)
      {
         j->config = status.config; j->generation = status.generation;
         retro_atomic_store_release_int(&j->configured, 1);
      }
      if (status.state >= RECORD_PYROWAVE_ENDED || failed)
      {
         if (failed || status.state == RECORD_PYROWAVE_FAILED || status.state == RECORD_PYROWAVE_REJECTED)
            terminal = RECORD_PYROWAVE_DISPATCH_FAILED;
         else if (status.state == RECORD_PYROWAVE_CANCELLED) terminal = RECORD_PYROWAVE_DISPATCH_CANCELLED;
         if (record_pyrowave_connection_free(j->connection))
         {
            j->connection = NULL;
            break;
         }
         state = RECORD_PYROWAVE_DISPATCH_DRAINING;
      }
      else if (status.state == RECORD_PYROWAVE_CONFIGURING) state = RECORD_PYROWAVE_DISPATCH_CONFIGURING;
      else if (status.state == RECORD_PYROWAVE_ACTIVE) state = RECORD_PYROWAVE_DISPATCH_READY;
      else if (status.state == RECORD_PYROWAVE_DRAINING) state = RECORD_PYROWAVE_DISPATCH_DRAINING;
      else state = RECORD_PYROWAVE_DISPATCH_STARTING;
      retro_atomic_store_release_int(&j->state, (int)state);
      if (!record_pyrowave_dispatch_wait(j, &readiness))
      {
         failed = true;
         record_pyrowave_connection_abort(j->connection);
         /* The source wake remains the retirement signal after socket failure. */
      }
   }
   record_pyrowave_dispatch_disarm(j);
   retro_atomic_store_release_int(&j->state, (int)terminal);
   /* Last access: a detached POSIX worker may still be returning on its stack. */
   retro_atomic_store_release_int(&j->finished, 1);
}

#if defined(_WIN32)
static DWORD WINAPI record_pyrowave_dispatch_thread(LPVOID user)
{
   record_pyrowave_dispatch_run((record_pyrowave_dispatch_t*)user);
   return 0;
}
#else
static void *record_pyrowave_dispatch_thread(void *user)
{
   record_pyrowave_dispatch_run((record_pyrowave_dispatch_t*)user);
   return NULL;
}
#endif

record_pyrowave_dispatch_t *record_pyrowave_dispatch_new(const void *address, size_t address_bytes,
      bool listen, const rpyrowave_stream_config_t *cfg, uint64_t generation,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm)
{
   record_pyrowave_dispatch_t *j = (record_pyrowave_dispatch_t*)calloc(1, sizeof(*j));
   if (!j) return NULL;
#if defined(_WIN32)
   j->registered_socket = INVALID_SOCKET;
   j->network_event = WSA_INVALID_EVENT;
#else
   j->wake_read = j->wake_write = -1;
#endif
   retro_atomic_int_init(&j->state, RECORD_PYROWAVE_DISPATCH_STARTING);
   retro_atomic_int_init(&j->reason, 0); retro_atomic_int_init(&j->cancel, 0);
   retro_atomic_int_init(&j->finish, 0); retro_atomic_int_init(&j->abandon, 0);
   retro_atomic_int_init(&j->decision, 0); retro_atomic_int_init(&j->configured, cfg ? 1 : 0);
   retro_atomic_int_init(&j->finished, 0);
   j->sender = cfg != NULL; j->listen = listen;
   if (cfg) j->config = *cfg;
   j->generation = generation;
   j->connection = record_pyrowave_connection_new(address, address_bytes, listen, cfg, generation, video, pcm);
   if (!j->connection) goto error;
   if (listen)
   {
#if defined(_WIN32)
      int bytes = sizeof(j->bound_address);
      if (getsockname((SOCKET)record_pyrowave_connection_handle(j->connection),
               (struct sockaddr*)&j->bound_address, &bytes)) goto error;
#else
      socklen_t bytes = sizeof(j->bound_address);
      if (getsockname((int)record_pyrowave_connection_handle(j->connection),
               (struct sockaddr*)&j->bound_address, &bytes)) goto error;
#endif
      if (!bytes || (size_t)bytes > sizeof(j->bound_address)) goto error;
      j->bound_bytes = (size_t)bytes;
   }
#if defined(_WIN32)
   j->wake = CreateEventA(NULL, FALSE, FALSE, NULL);
   j->network_event = WSACreateEvent();
   if (!j->wake || j->network_event == WSA_INVALID_EVENT) goto error;
   j->thread = CreateThread(NULL, 0, record_pyrowave_dispatch_thread, j, 0, NULL);
   if (!j->thread) goto error;
#else
   {
      int descriptors[2], result;
      pthread_attr_t attributes;
      pthread_t thread;
      if (pipe(descriptors)) goto error;
      j->wake_read = descriptors[0]; j->wake_write = descriptors[1];
      if (fcntl(j->wake_read, F_SETFL, O_NONBLOCK) || fcntl(j->wake_write, F_SETFL, O_NONBLOCK)) goto error;
      if (fcntl(j->wake_read, F_SETFD, FD_CLOEXEC) || fcntl(j->wake_write, F_SETFD, FD_CLOEXEC)) goto error;
      if (pthread_attr_init(&attributes)) goto error;
      result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
      if (!result) result = pthread_create(&thread, &attributes, record_pyrowave_dispatch_thread, j);
      pthread_attr_destroy(&attributes);
      if (result) goto error;
   }
#endif
   return j;
error:
   retro_atomic_store_release_int(&j->finished, 1);
   record_pyrowave_dispatch_free(j);
   return NULL;
}

unsigned record_pyrowave_dispatch_state(const record_pyrowave_dispatch_t *j, unsigned *details)
{
   if (details) *details = j ? (unsigned)retro_atomic_load_acquire_int(&j->reason) : 0;
   return j ? (unsigned)retro_atomic_load_acquire_int(&j->state) : RECORD_PYROWAVE_DISPATCH_FAILED;
}

bool record_pyrowave_dispatch_bound_address(const record_pyrowave_dispatch_t *j, void *address, size_t *bytes)
{
   size_t capacity;
   if (!j || !bytes) return false;
   capacity = *bytes; *bytes = j->bound_bytes;
   if (!address || !j->bound_bytes || capacity < j->bound_bytes) return false;
   memcpy(address, &j->bound_address, j->bound_bytes);
   return true;
}

bool record_pyrowave_dispatch_config(const record_pyrowave_dispatch_t *j,
      rpyrowave_stream_config_t *cfg, uint64_t *generation)
{
   if (!j || !cfg || !generation || !retro_atomic_load_acquire_int(&j->configured)) return false;
   *cfg = j->config; *generation = j->generation;
   return true;
}

static bool record_pyrowave_dispatch_decide(record_pyrowave_dispatch_t *j,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm, unsigned reason)
{
   if (!j || j->sender || record_pyrowave_dispatch_state(j, NULL) != RECORD_PYROWAVE_DISPATCH_CONFIGURING
         || (!reason && !video) || !retro_atomic_cas_int(&j->decision, 0, 1)) return false;
   if (video) j->decision_video = *video;
   j->decision_pcm = pcm; j->decision_reason = reason;
   retro_atomic_store_release_int(&j->decision, 2);
   record_pyrowave_dispatch_notify(j);
   return true;
}

bool record_pyrowave_dispatch_accept(record_pyrowave_dispatch_t *j,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm)
{
   return record_pyrowave_dispatch_decide(j, video, pcm, 0);
}

bool record_pyrowave_dispatch_reject(record_pyrowave_dispatch_t *j, unsigned reason)
{
   return reason && record_pyrowave_dispatch_decide(j, NULL, NULL, reason);
}

bool record_pyrowave_dispatch_finish(record_pyrowave_dispatch_t *j)
{
   if (!j || !j->sender || record_pyrowave_dispatch_state(j, NULL) >= RECORD_PYROWAVE_DISPATCH_DONE) return false;
   retro_atomic_store_release_int(&j->finish, 1); record_pyrowave_dispatch_notify(j);
   return true;
}

void record_pyrowave_dispatch_cancel(record_pyrowave_dispatch_t *j)
{
   if (!j) return;
   retro_atomic_store_release_int(&j->cancel, 1); record_pyrowave_dispatch_notify(j);
}

void record_pyrowave_dispatch_abandon_video(record_pyrowave_dispatch_t *j)
{
   if (!j) return;
   retro_atomic_store_release_int(&j->abandon, 1); record_pyrowave_dispatch_cancel(j);
}

bool record_pyrowave_dispatch_quiesced(const record_pyrowave_dispatch_t *j)
{
   return !j || retro_atomic_load_acquire_int(&j->finished) != 0;
}

bool record_pyrowave_dispatch_free(record_pyrowave_dispatch_t *j)
{
   if (!j) return true;
   if (!retro_atomic_load_acquire_int(&j->finished)) return false;
#if defined(_WIN32)
   if (j->thread && WaitForSingleObject(j->thread, 0) != WAIT_OBJECT_0) return false;
   if (j->thread) CloseHandle(j->thread);
   if (j->wake) CloseHandle(j->wake);
   if (j->network_event != WSA_INVALID_EVENT) WSACloseEvent(j->network_event);
#else
   if (j->wake_read >= 0) close(j->wake_read);
   if (j->wake_write >= 0) close(j->wake_write);
#endif
   if (!record_pyrowave_connection_free(j->connection)) return false;
   free(j);
   return true;
}
#else
bool record_pyrowave_dispatch_quiesced(const record_pyrowave_dispatch_t *j)
{
   (void)j; return true;
}
record_pyrowave_dispatch_t *record_pyrowave_dispatch_new(const void *address, size_t address_bytes,
      bool listen, const rpyrowave_stream_config_t *cfg, uint64_t generation,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm)
{
   (void)address; (void)address_bytes; (void)listen; (void)cfg;
   (void)generation; (void)video; (void)pcm;
   return NULL;
}
void record_pyrowave_dispatch_notify(record_pyrowave_dispatch_t *j)
{
   (void)j;
}
bool record_pyrowave_dispatch_bound_address(const record_pyrowave_dispatch_t *j, void *address, size_t *bytes)
{
   (void)j; (void)address;
   if (bytes) *bytes = 0;
   return false;
}
unsigned record_pyrowave_dispatch_state(const record_pyrowave_dispatch_t *j, unsigned *details)
{
   (void)j;
   if (details) *details = 0;
   return RECORD_PYROWAVE_DISPATCH_FAILED;
}
bool record_pyrowave_dispatch_config(const record_pyrowave_dispatch_t *j, rpyrowave_stream_config_t *cfg, uint64_t *generation)
{
   (void)j; (void)cfg; (void)generation;
   return false;
}
bool record_pyrowave_dispatch_accept(record_pyrowave_dispatch_t *j, const record_pyrowave_video_t *v, record_pyrowave_pcm_t *p)
{
   (void)j; (void)v; (void)p;
   return false;
}
bool record_pyrowave_dispatch_reject(record_pyrowave_dispatch_t *j, unsigned reason)
{
   (void)j; (void)reason;
   return false;
}
bool record_pyrowave_dispatch_finish(record_pyrowave_dispatch_t *j)
{
   (void)j;
   return false;
}
void record_pyrowave_dispatch_cancel(record_pyrowave_dispatch_t *j)
{
   (void)j;
}
void record_pyrowave_dispatch_abandon_video(record_pyrowave_dispatch_t *j)
{
   (void)j;
}
bool record_pyrowave_dispatch_free(record_pyrowave_dispatch_t *j)
{
   return !j;
}
#endif
