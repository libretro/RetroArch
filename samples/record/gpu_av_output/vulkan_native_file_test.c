/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "../../../record/record_file_posix.h"
static uint8_t payload[200000];
static void verify(const char *path,size_t size)
{
   FILE *input=fopen(path,"rb");
   size_t i;
   assert(input);
   for (i=0;i<size;i++) assert(fgetc(input)==payload[i]);
   assert(fgetc(input)==EOF && !ferror(input) && !fclose(input));
}
int main(void)
{
   record_file_posix_t *f;
   size_t i;
   unsigned writes=0;
   int result;
   for (i=0;i<sizeof(payload);i++) payload[i]=(uint8_t)(i*13+i/251);
   f=record_file_posix_new("record-output.bin"); assert(f);
   assert(!record_file_posix_new("record-output.bin"));
   assert(record_file_posix_push(f,payload,sizeof(payload))==RECORD_FILE_POSIX_OK);
   do
   {
      result=record_file_posix_step(f); assert(++writes<=4);
   } while (result==RECORD_FILE_POSIX_BUSY);
   assert(result==RECORD_FILE_POSIX_OK && writes==4 && record_file_posix_bytes(f)==sizeof(payload));
   assert(record_file_posix_finish(f) && !record_file_posix_error(f) && record_file_posix_free(f));
   assert(!record_file_posix_new("record-output.bin")); verify("record-output.bin",sizeof(payload));
   f=record_file_posix_new("record-cancel.bin"); assert(f);
   assert(record_file_posix_push(f,payload,sizeof(payload))==RECORD_FILE_POSIX_OK);
   assert(record_file_posix_step(f)==RECORD_FILE_POSIX_BUSY && record_file_posix_bytes(f)==65536);
   assert(record_file_posix_cancel(f) && !record_file_posix_finish(f) && record_file_posix_free(f));
   verify("record-cancel.bin",65536);
   puts("Native recording file exclusive-create/borrow/write/cancel PASS"); return 0;
}
