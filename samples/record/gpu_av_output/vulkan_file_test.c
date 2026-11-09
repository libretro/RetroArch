/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../../../record/record_mediacodec_file.h"
static unsigned allocations, fail_allocation;
static void *test_calloc(size_t count, size_t size)
{
   return ++allocations == fail_allocation ? NULL : calloc(count, size);
}
static long test_write(int fd, const void *data, size_t bytes);
#define RECORD_FP_WRITE test_write
#define RECORD_FILE_POSIX_TEST
#define calloc test_calloc
#include "../../../record/record_file_posix.c"
#undef calloc
static unsigned opened, closed, writes, synced, new_failure, close_failure, write_mode;
static size_t consumed;
static const uint8_t payload[200000] = {0};
struct record_mediacodec_file
{
   int active;
};
static struct record_mediacodec_file native_file;
record_mediacodec_file_t *record_mediacodec_file_new(const char *path, int *error)
{
   assert(path && !strcmp(path, "synthetic.mp4") && !native_file.active);
   opened++;
   *error = new_failure ? EACCES : 0;
   if (new_failure)
      return NULL;
   native_file.active = 1;
   return &native_file;
}
int record_mediacodec_file_descriptor(const record_mediacodec_file_t *file)
{
   assert(file == &native_file && file->active);
   return 71;
}
bool record_mediacodec_file_close(record_mediacodec_file_t *file, bool complete,
                                  int *error)
{
   if (error)
      *error = 0;
   if (!file)
      return true;
   assert(file == &native_file && file->active);
   file->active = 0;
   closed++;
   if (complete)
      synced++;
   if (close_failure)
   {
      if (error)
         *error = EIO;
      return false;
   }
   return true;
}
static long test_write(int fd, const void *data, size_t bytes)
{
   size_t output = bytes;
   assert(fd == 71 && data == payload + consumed && bytes && bytes <= 65536);
   writes++;
   if (write_mode == 1)
   {
      errno = EINTR;
      return -1;
   }
   if (write_mode == 2)
   {
      errno = EAGAIN;
      return -1;
   }
   if (write_mode == 3)
   {
      errno = ENOSPC;
      return -1;
   }
   if (write_mode == 4)
      return 0;
   if (write_mode == 5)
      return (long)bytes + 1;
   if (write_mode == 6 && output > 32768)
      output = 32768;
   consumed += output;
   return (long)output;
}
static void setup(void)
{
   assert(!native_file.active);
   allocations = fail_allocation = opened = closed = writes = synced = new_failure =
       close_failure = write_mode = 0;
   consumed = 0;
}
static void normal(void)
{
   record_file_posix_t *f;
   unsigned i;
   setup();
   f = record_file_posix_new("synthetic.mp4");
   assert(f && allocations == 1 && opened == 1);
   assert(record_file_posix_step(f) == RECORD_FILE_POSIX_OK && !writes);
   assert(record_file_posix_push(f, payload, sizeof(payload)) == RECORD_FILE_POSIX_OK &&
          !writes);
   assert(record_file_posix_push(f, payload, 1) == RECORD_FILE_POSIX_BUSY);
   assert(!record_file_posix_finish(f) && !record_file_posix_free(f) && !synced);
   for (i = 1; i <= 2; i++)
   {
      write_mode = i;
      assert(record_file_posix_step(f) == RECORD_FILE_POSIX_BUSY);
      assert(!record_file_posix_bytes(f) && !record_file_posix_error(f));
   }
   write_mode = 6;
   for (i = 0; i < 6; i++)
   {
      assert(record_file_posix_step(f) == RECORD_FILE_POSIX_BUSY);
      assert(record_file_posix_bytes(f) == 32768 * (i + 1));
   }
   assert(record_file_posix_step(f) == RECORD_FILE_POSIX_OK &&
          record_file_posix_bytes(f) == sizeof(payload));
   assert(allocations == 1 && writes == 9 && !record_file_posix_error(f));
   assert(record_file_posix_finish(f) && record_file_posix_finish(f) && synced == 1 &&
          closed == 1);
   assert(!record_file_posix_cancel(f) &&
          record_file_posix_push(f, payload, 1) == RECORD_FILE_POSIX_ERROR);
   assert(record_file_posix_free(f) && closed == 1);
}
static void failures(void)
{
   record_file_posix_t *f;
   unsigned mode;
   for (mode = 3; mode <= 5; mode++)
   {
      setup();
      f = record_file_posix_new("synthetic.mp4");
      assert(f);
      assert(record_file_posix_push(f, payload, sizeof(payload)) ==
             RECORD_FILE_POSIX_OK);
      write_mode = mode;
      assert(record_file_posix_step(f) == RECORD_FILE_POSIX_ERROR);
      assert(record_file_posix_error(f) == (mode == 3 ? ENOSPC : EIO));
      assert(!record_file_posix_finish(f) &&
             record_file_posix_step(f) == RECORD_FILE_POSIX_ERROR && writes == 1);
      assert(record_file_posix_free(f) && closed == 1 && !synced);
   }
   setup();
   f = record_file_posix_new("synthetic.mp4");
   assert(f);
   close_failure = 1;
   assert(!record_file_posix_finish(f) && record_file_posix_error(f) == EIO);
   assert(!record_file_posix_finish(f) && closed == 1 && synced == 1 &&
          record_file_posix_free(f));
   setup();
   f = record_file_posix_new("synthetic.mp4");
   assert(f);
   assert(record_file_posix_push(f, payload, 1) == RECORD_FILE_POSIX_OK);
   assert(record_file_posix_cancel(f) && record_file_posix_error(f) == ECANCELED &&
          !writes);
   assert(!record_file_posix_finish(f) && record_file_posix_free(f) && closed == 1 &&
          !synced);
   setup();
   fail_allocation = 1;
   assert(!record_file_posix_new("synthetic.mp4") && !opened);
   setup();
   new_failure = 1;
   assert(!record_file_posix_new("synthetic.mp4") && opened == 1 && !closed);
   assert(record_file_posix_free(NULL) && !record_file_posix_finish(NULL));
}
int main(void)
{
   normal();
   failures();
   puts("POSIX recording file borrow/short-write/error/finalize PASS");
   return 0;
}
