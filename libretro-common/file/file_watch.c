/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_watch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <file/file_watch.h>
#include <lists/string_list.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#if defined(__linux__)
#include <linux/version.h>
/* inotify API was added in 2.6.13 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
#define FILE_WATCH_INOTIFY
#endif
#elif defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#define FILE_WATCH_WIN32
#endif

#if defined(FILE_WATCH_INOTIFY)

#include <fcntl.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/utsname.h>

/* Large enough for one event with a maximal name; the poll returns on
 * the first matching event and drains the rest on later polls, so the
 * buffer only bounds how much one read() consumes. */
#define FILE_WATCH_READ_BUF_LEN 4096

struct file_watch
{
   int fd;
   uint32_t mask;
   /* Owned read buffer for the per-frame poll, so polling costs one
    * non-blocking read() and no stack traffic. */
   char *buf;
   int *wds;
   unsigned wd_count;
};

bool file_watch_supported(void)
{
   /* Guard against running on a kernel older than the headers this
    * was built with. */
   struct utsname buffer;
   int major = 0, minor = 0;
   unsigned krel = 0;
   char *ptr;
   char *end = NULL;

   if (uname(&buffer) != 0)
      return false;

   ptr   = buffer.release;
   major = (int)strtoul(ptr, &end, 10);
   if (end && *end == '.')
   {
      ptr   = end + 1;
      minor = (int)strtoul(ptr, &end, 10);
   }
   if (end && *end == '.')
   {
      ptr  = end + 1;
      krel = (unsigned)strtoul(ptr, &end, 10);
   }

   if (major < 2 || (major == 2 && (minor < 6 || (minor == 6 && krel < 13))))
      return false;
   return true;
}

file_watch_t *file_watch_new(struct string_list *paths, int event_mask)
{
   file_watch_t *watch;
   uint32_t inotify_mask = 0;
   unsigned added        = 0;
   int fd;
   size_t i;

   if (!paths || paths->size == 0 || !file_watch_supported())
      return NULL;

   fd = inotify_init();
   if (fd < 0)
      return NULL;

   if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK))
   {
      close(fd);
      return NULL;
   }

   if (!(watch = (file_watch_t*)calloc(1, sizeof(*watch))))
   {
      close(fd);
      return NULL;
   }
   watch->fd = fd;

   if (!(watch->wds = (int*)calloc(paths->size, sizeof(int))))
   {
      free(watch);
      close(fd);
      return NULL;
   }

   if (!(watch->buf = (char*)malloc(FILE_WATCH_READ_BUF_LEN)))
   {
      free(watch->wds);
      free(watch);
      close(fd);
      return NULL;
   }

   if (event_mask & FILE_WATCH_EVENT_MODIFIED)
      inotify_mask |= IN_MODIFY;
   if (event_mask & FILE_WATCH_EVENT_WRITE_FILE_CLOSED)
      inotify_mask |= IN_CLOSE_WRITE;
   if (event_mask & FILE_WATCH_EVENT_FILE_MOVED)
      inotify_mask |= IN_MOVE_SELF;
   if (event_mask & FILE_WATCH_EVENT_FILE_DELETED)
      inotify_mask |= IN_DELETE_SELF;

   watch->mask = inotify_mask;

   for (i = 0; i < paths->size; i++)
   {
      int wd = inotify_add_watch(fd, paths->elems[i].data, inotify_mask);

      if (wd < 0)
         continue;

      watch->wds[watch->wd_count++] = wd;
      added++;
   }

   if (!added)
   {
      file_watch_free(watch);
      return NULL;
   }
   return watch;
}

bool file_watch_poll(file_watch_t *watch)
{
   char *buf;
   int length;

   if (!watch)
      return false;

   buf = watch->buf;

   /* This runs on the frame loop, once per frame while file watching
    * is enabled.  The idle cost is a single non-blocking read()
    * returning EAGAIN.  When an event arrives, the first matching one
    * answers the poll; a re-read of the changed file sees the new
    * contents through the page cache without any explicit flush, so
    * there is nothing to do here beyond the match. */
   while ((length = read(watch->fd, buf, FILE_WATCH_READ_BUF_LEN)) > 0)
   {
      int i = 0;

      while (i < length)
      {
         struct inotify_event *event = (struct inotify_event *)&buf[i];

         /* A kernel queue overflow means events were dropped:
          * something may have changed without a record of it.
          * Report a change rather than miss one, matching the
          * notification-buffer-overflow handling on Windows, so
          * a consumer can rely on the poll for conservative
          * cache invalidation. */
         if (event->mask & (watch->mask | IN_Q_OVERFLOW))
            return true;

         i += sizeof(struct inotify_event) + event->len;
      }
   }
   return false;
}

void file_watch_free(file_watch_t *watch)
{
   unsigned i;

   if (!watch)
      return;

   for (i = 0; i < watch->wd_count; i++)
      inotify_rm_watch(watch->fd, watch->wds[i]);
   free(watch->wds);
   free(watch->buf);
   close(watch->fd);
   free(watch);
}

#elif defined(__APPLE__) && defined(HAVE_GCD)

#include <fcntl.h>
#include <unistd.h>
#include <dispatch/dispatch.h>

#include <retro_atomic.h>

/* Per-file GCD vnode dispatch sources.  The event handlers run on the
 * global concurrent queue and communicate with the polling thread
 * through a monotonic atomic counter; the reader compares it against
 * a reader-side cursor.  Handlers are registered through the _f
 * function-pointer variants with the watch entry as the dispatch
 * context, so this TU stays plain C without blocks. */

typedef struct file_watch_entry
{
   int fd;                    /* Opened with O_EVTONLY */
   dispatch_source_t source;
   /* Per-entry semaphore signalled from the source's cancel
    * handler.  Needed because dispatch_source_cancel is
    * asynchronous: it flags the source for cancellation but
    * any already-dispatched event handler invocation keeps
    * running to completion on its target queue, which is the
    * global concurrent queue here.  The event handler
    * dereferences the owner's event_count, so the owner cannot
    * be freed until no in-flight handler remains.  The cancel
    * handler fires once all handler invocations have drained,
    * so waiting on this semaphore before free() is the
    * standard safe-teardown pattern for dispatch sources. */
   dispatch_semaphore_t cancel_sem;
   struct file_watch *owner;
} file_watch_entry_t;

struct file_watch
{
   dispatch_queue_t queue;    /* Global queue; never released */
   file_watch_entry_t *watches;
   size_t watch_count;
   /* Monotonic event counter.  The producer (GCD event handler
    * thread) increments via retro_atomic_fetch_add_int when the
    * filesystem reports an event; the reader (polling thread)
    * acquire-loads it and compares against last_seen below.  A
    * counter rather than a flag: the "did anything change?"
    * semantic is (now != last_seen), no read-and-clear CAS is
    * needed because last_seen is reader-thread-only state. */
   retro_atomic_int_t event_count;
   /* Reader-side cursor; not atomic. */
   int last_seen;
};

bool file_watch_supported(void)
{
   return true;
}

static void file_watch_darwin_event(void *ctx)
{
   file_watch_entry_t *entry = (file_watch_entry_t*)ctx;
   retro_atomic_fetch_add_int(&entry->owner->event_count, 1);
}

/* Signals cancel_sem.  file_watch_free cancels the source and waits
 * on this semaphore, guaranteeing all pending event handlers have
 * completed before the watch is freed. */
static void file_watch_darwin_cancelled(void *ctx)
{
   file_watch_entry_t *entry = (file_watch_entry_t*)ctx;
   dispatch_semaphore_signal(entry->cancel_sem);
}

/* Does NOT release watch->queue: that slot holds the handle returned
 * by dispatch_get_global_queue(), which is a process-wide singleton
 * that must never be released - dispatch_release on a global queue
 * is documented as undefined behaviour. */
void file_watch_free(file_watch_t *watch)
{
   size_t i;

   if (!watch)
      return;

   if (watch->watches)
   {
      for (i = 0; i < watch->watch_count; i++)
      {
         if (watch->watches[i].source)
         {
            /* Cancel the source, then wait for the cancel handler
             * to fire.  dispatch_source_cancel is asynchronous -
             * it only marks the source as cancelled; the cancel
             * handler is guaranteed to fire AFTER all pending
             * event handler invocations have drained.  Without
             * this wait a racing event handler would write into
             * freed memory for event_count. */
            dispatch_source_cancel(watch->watches[i].source);
            if (watch->watches[i].cancel_sem)
            {
               dispatch_semaphore_wait(
                     watch->watches[i].cancel_sem,
                     DISPATCH_TIME_FOREVER);
            }
            /* Plain C TU: dispatch objects are not under ARC here,
             * so dispatch_release is always the correct release. */
            dispatch_release(watch->watches[i].source);
            dispatch_release(watch->watches[i].cancel_sem);
         }
         if (watch->watches[i].fd >= 0)
            close(watch->watches[i].fd);
      }
      free(watch->watches);
   }
   free(watch);
}

file_watch_t *file_watch_new(struct string_list *paths, int event_mask)
{
   file_watch_t *watch;
   unsigned long vnode_flags = 0;
   unsigned armed            = 0;
   size_t i;

   if (!paths || paths->size == 0)
      return NULL;

   if (!(watch = (file_watch_t*)calloc(1, sizeof(*watch))))
      return NULL;

   watch->queue       = dispatch_get_global_queue(
         DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
   watch->watch_count = paths->size;
   /* watch was calloc'd, so event_count and last_seen are already
    * zero-bit; but plain assignment to a retro_atomic_int_t is
    * illegal under the C11 stdatomic backend, so use the init
    * helper for the atomic field. */
   retro_atomic_int_init(&watch->event_count, 0);
   watch->last_seen   = 0;

   if (!(watch->watches = (file_watch_entry_t*)calloc(
         paths->size, sizeof(file_watch_entry_t))))
   {
      free(watch);
      return NULL;
   }

   if (event_mask & FILE_WATCH_EVENT_MODIFIED)
      vnode_flags |= DISPATCH_VNODE_WRITE;
   if (event_mask & FILE_WATCH_EVENT_WRITE_FILE_CLOSED)
      vnode_flags |= DISPATCH_VNODE_ATTRIB; /* mtime changes on close */
   if (event_mask & FILE_WATCH_EVENT_FILE_MOVED)
      vnode_flags |= DISPATCH_VNODE_RENAME;
   if (event_mask & FILE_WATCH_EVENT_FILE_DELETED)
      vnode_flags |= DISPATCH_VNODE_DELETE;

   for (i = 0; i < paths->size; i++)
   {
      const char *path = paths->elems[i].data;
      int fd           = open(path, O_EVTONLY);

      watch->watches[i].fd         = fd;
      watch->watches[i].source     = NULL;
      watch->watches[i].cancel_sem = NULL;
      watch->watches[i].owner      = watch;

      if (fd >= 0)
      {
         dispatch_source_t    source;
         dispatch_semaphore_t cancel_sem;

         /* Create the cancel semaphore up-front.  If this fails
          * (realistically only on OOM) we skip source creation
          * entirely rather than create a source we cannot safely
          * tear down - without a semaphore the free path has no
          * way to wait for in-flight event handlers to drain
          * before the free(). */
         cancel_sem = dispatch_semaphore_create(0);
         if (!cancel_sem)
         {
            close(fd);
            watch->watches[i].fd = -1;
            continue;
         }

         source = dispatch_source_create(
               DISPATCH_SOURCE_TYPE_VNODE, fd, vnode_flags,
               watch->queue);

         if (source)
         {
            dispatch_set_context(source, &watch->watches[i]);
            dispatch_source_set_event_handler_f(source,
                  file_watch_darwin_event);
            dispatch_source_set_cancel_handler_f(source,
                  file_watch_darwin_cancelled);

            watch->watches[i].source     = source;
            watch->watches[i].cancel_sem = cancel_sem;
            dispatch_resume(source);
            armed++;
         }
         else
         {
            dispatch_release(cancel_sem);
            close(fd);
            watch->watches[i].fd = -1;
         }
      }
   }

   if (!armed)
   {
      file_watch_free(watch);
      return NULL;
   }
   return watch;
}

bool file_watch_poll(file_watch_t *watch)
{
   int now;
   bool changed;

   if (!watch)
      return false;

   /* Acquire-load the producer's counter and compare against the
    * reader-side cursor.  The acquire-load pairs with the
    * producer's fetch_add.  The counter is monotonic and never
    * reset; the comparison remains correct across wraparound under
    * modular int arithmetic - any non-zero (now - last_seen) means
    * the producer advanced. */
   now              = retro_atomic_load_acquire_int(&watch->event_count);
   changed          = (now != watch->last_seen);
   watch->last_seen = now;
   return changed;
}

#elif defined(FILE_WATCH_WIN32)

#include <windows.h>

#include <encodings/utf.h>

/* Windows watches directories, not files, so the watched paths are
 * grouped by parent directory; each directory gets an overlapped
 * ReadDirectoryChangesW with an event-carrying OVERLAPPED.  The
 * per-frame poll is a zero-timeout wait on those events, and a
 * completed notification is filtered against the watched basenames.
 * Editors that save atomically (write a temp file, rename it over the
 * target) report the save as a rename onto the watched name rather
 * than a plain write, so a rename onto a watched name counts as a
 * modification. */

#define FILE_WATCH_BUF_LEN 4096

typedef struct file_watch_dir
{
   char *dir_path;             /* UTF-8, for grouping */
   HANDLE dir;
   HANDLE evt;                 /* manual-reset completion event */
   OVERLAPPED ovl;
   char *buf;                  /* notification buffer, heap-owned */
   struct string_list *names;  /* watched basenames within this dir */
} file_watch_dir_t;

struct file_watch
{
   int flags;
   DWORD notify_filter;
   unsigned dir_count;
   file_watch_dir_t *dirs;
};

bool file_watch_supported(void)
{
   return true;
}

/* Re-issues the overlapped watch; required after every completion or
 * the directory silently stops being watched. */
static bool file_watch_arm(file_watch_dir_t *d, DWORD filter)
{
   ResetEvent(d->evt);
   memset(&d->ovl, 0, sizeof(d->ovl));
   d->ovl.hEvent = d->evt;
   return ReadDirectoryChangesW(d->dir, d->buf, FILE_WATCH_BUF_LEN,
         FALSE, filter, NULL, &d->ovl, NULL) != 0;
}

static bool file_watch_action_matches(int flags, DWORD action)
{
   if (flags & (FILE_WATCH_EVENT_MODIFIED
              | FILE_WATCH_EVENT_WRITE_FILE_CLOSED))
   {
      if (   action == FILE_ACTION_MODIFIED
          || action == FILE_ACTION_ADDED
          || action == FILE_ACTION_RENAMED_NEW_NAME)
         return true;
   }
   if (flags & FILE_WATCH_EVENT_FILE_MOVED)
   {
      if (   action == FILE_ACTION_RENAMED_OLD_NAME
          || action == FILE_ACTION_RENAMED_NEW_NAME)
         return true;
   }
   if (flags & FILE_WATCH_EVENT_FILE_DELETED)
   {
      if (   action == FILE_ACTION_REMOVED
          || action == FILE_ACTION_RENAMED_OLD_NAME)
         return true;
   }
   return false;
}

/* Tears down every watch; safe on partially constructed state. */
void file_watch_free(file_watch_t *watch)
{
   unsigned i;

   if (!watch)
      return;

   if (watch->dirs)
   {
      for (i = 0; i < watch->dir_count; i++)
      {
         file_watch_dir_t *d = &watch->dirs[i];

         if (d->dir && d->dir != INVALID_HANDLE_VALUE)
         {
            DWORD bytes = 0;

            /* The watch is issued and cancelled on the same thread,
             * so CancelIo suffices; drain the completion before the
             * buffer and OVERLAPPED go away. */
            CancelIo(d->dir);
            GetOverlappedResult(d->dir, &d->ovl, &bytes, TRUE);
            CloseHandle(d->dir);
         }
         if (d->evt)
            CloseHandle(d->evt);
         if (d->buf)
            free(d->buf);
         if (d->names)
            string_list_free(d->names);
         if (d->dir_path)
            free(d->dir_path);
      }
      free(watch->dirs);
   }
   free(watch);
}

file_watch_t *file_watch_new(struct string_list *paths, int event_mask)
{
   file_watch_t *watch = NULL;
   char *dir_scratch   = NULL;
   DWORD filter        = 0;
   unsigned armed      = 0;
   unsigned j;
   size_t i;

   if (!paths || paths->size == 0)
      return NULL;

   /* The rename bits are needed even for pure write watching, because
    * atomic saves surface as renames onto the watched name. */
   if (event_mask & (FILE_WATCH_EVENT_MODIFIED
                   | FILE_WATCH_EVENT_WRITE_FILE_CLOSED))
      filter |= FILE_NOTIFY_CHANGE_LAST_WRITE
              | FILE_NOTIFY_CHANGE_SIZE
              | FILE_NOTIFY_CHANGE_FILE_NAME;
   if (event_mask & (FILE_WATCH_EVENT_FILE_MOVED
                   | FILE_WATCH_EVENT_FILE_DELETED))
      filter |= FILE_NOTIFY_CHANGE_FILE_NAME;

   if (!(watch = (file_watch_t*)calloc(1, sizeof(*watch))))
      return NULL;
   watch->flags         = event_mask;
   watch->notify_filter = filter;

   if (!(watch->dirs = (file_watch_dir_t*)
         calloc(paths->size, sizeof(*watch->dirs))))
   {
      free(watch);
      return NULL;
   }

   if (!(dir_scratch = (char*)malloc(PATH_MAX_LENGTH)))
   {
      free(watch->dirs);
      free(watch);
      return NULL;
   }

   /* Group the watched files by parent directory. */
   for (i = 0; i < paths->size; i++)
   {
      const char *path     = paths->elems[i].data;
      const char *base     = path_basename_nocompression(path);
      file_watch_dir_t *d  = NULL;
      union string_list_elem_attr attr;

      attr.i = 0;
      fill_pathname_basedir(dir_scratch, path, PATH_MAX_LENGTH);

      for (j = 0; j < watch->dir_count; j++)
      {
         if (string_is_equal_noncase(watch->dirs[j].dir_path, dir_scratch))
         {
            d = &watch->dirs[j];
            break;
         }
      }

      if (!d)
      {
         d = &watch->dirs[watch->dir_count];
         if (!(d->dir_path = strdup(dir_scratch)))
            continue;
         if (!(d->names = string_list_new()))
         {
            free(d->dir_path);
            d->dir_path = NULL;
            continue;
         }
         watch->dir_count++;
      }

      string_list_append(d->names, base, attr);
   }

   free(dir_scratch);

   /* Open and arm each directory. */
   for (j = 0; j < watch->dir_count; j++)
   {
      file_watch_dir_t *d = &watch->dirs[j];
      wchar_t *dir_w      = utf8_to_utf16_string_alloc(d->dir_path);

      if (!dir_w)
         continue;

      d->dir = CreateFileW(dir_w, FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
      free(dir_w);

      if (d->dir == INVALID_HANDLE_VALUE)
         continue;

      if (!(d->evt = CreateEventW(NULL, TRUE, FALSE, NULL)))
      {
         CloseHandle(d->dir);
         d->dir = NULL;
         continue;
      }

      if (!(d->buf = (char*)malloc(FILE_WATCH_BUF_LEN)))
      {
         CloseHandle(d->evt);
         CloseHandle(d->dir);
         d->evt = NULL;
         d->dir = NULL;
         continue;
      }

      if (!file_watch_arm(d, filter))
      {
         free(d->buf);
         CloseHandle(d->evt);
         CloseHandle(d->dir);
         d->buf = NULL;
         d->evt = NULL;
         d->dir = NULL;
         continue;
      }

      armed++;
   }

   if (!armed)
   {
      file_watch_free(watch);
      return NULL;
   }
   return watch;
}

bool file_watch_poll(file_watch_t *watch)
{
   bool matched = false;
   unsigned i;

   if (!watch)
      return false;

   /* This runs on the frame loop, so the idle path is checked rather
    * than waited on. A zero-timeout WaitForSingleObject() is not a
    * cheap test: it has no user-mode fast path on any Windows version
    * and enters the kernel through NtWaitForSingleObject() whatever
    * the object's state, so polling one per directory per frame is a
    * syscall per directory per frame. HasOverlappedIoCompleted() reads
    * OVERLAPPED::Internal directly, which the I/O manager writes when
    * the operation completes, and costs a load.
    *
    * The two are interchangeable here. The event is manual-reset and
    * file_watch_arm() resets it explicitly before re-issuing, so
    * nothing relied on the wait clearing it. */
   for (i = 0; i < watch->dir_count; i++)
   {
      file_watch_dir_t *d = &watch->dirs[i];
      DWORD bytes         = 0;

      if (!d->dir || d->dir == INVALID_HANDLE_VALUE)
         continue;
      if (!HasOverlappedIoCompleted(&d->ovl))
         continue;

      if (GetOverlappedResult(d->dir, &d->ovl, &bytes, FALSE))
      {
         if (bytes == 0)
         {
            /* Notification buffer overflowed: something changed but
             * the details are gone.  Report a change rather than
             * miss one. */
            matched = true;
         }
         else
         {
            FILE_NOTIFY_INFORMATION *fni =
                  (FILE_NOTIFY_INFORMATION*)d->buf;

            for (;;)
            {
               size_t name_chars = fni->FileNameLength / sizeof(WCHAR);

               if (name_chars > 0 && name_chars <= MAX_PATH)
               {
                  wchar_t name_w[MAX_PATH + 1];
                  /* UTF-8 needs at most three octets per UTF-16 code
                   * unit (surrogate pairs use four octets for two
                   * units). */
                  char name_utf8[MAX_PATH * 3 + 1];

                  memcpy(name_w, fni->FileName,
                        name_chars * sizeof(WCHAR));
                  name_w[name_chars] = L'\0';
                  name_utf8[0]       = '\0';

                  if (utf16_to_char_string((const uint16_t*)name_w,
                        name_utf8, sizeof(name_utf8))
                        && file_watch_action_matches(watch->flags,
                              fni->Action))
                  {
                     size_t k;

                     for (k = 0; k < d->names->size; k++)
                     {
                        if (string_is_equal_noncase(
                              d->names->elems[k].data, name_utf8))
                        {
                           matched = true;
                           break;
                        }
                     }
                  }
               }

               if (matched || !fni->NextEntryOffset)
                  break;
               fni = (FILE_NOTIFY_INFORMATION*)
                     ((char*)fni + fni->NextEntryOffset);
            }
         }
      }

      /* Always re-arm after a completion, or watching stops. */
      if (!file_watch_arm(d, watch->notify_filter))
      {
         CancelIo(d->dir);
         CloseHandle(d->evt);
         CloseHandle(d->dir);
         free(d->buf);
         d->buf = NULL;
         d->evt = NULL;
         d->dir = NULL;
      }

      if (matched)
         return true;
   }

   return false;
}

#else

/* No file watching on this platform. */

bool file_watch_supported(void)
{
   return false;
}

file_watch_t *file_watch_new(struct string_list *paths, int event_mask)
{
   (void)paths;
   (void)event_mask;
   return NULL;
}

bool file_watch_poll(file_watch_t *watch)
{
   (void)watch;
   return false;
}

void file_watch_free(file_watch_t *watch)
{
   (void)watch;
}

#endif
