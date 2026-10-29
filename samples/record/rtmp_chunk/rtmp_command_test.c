/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_rtmp_command.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t golden[1024];
static char oversized[4098];

static record_rtmp_command_config_t config(unsigned command)
{
   record_rtmp_command_config_t c;
   c.application = "live";
   c.url = "rtmp://127.0.0.1/live";
   c.name = "test/\303\251";
   c.transaction = command == RECORD_RTMP_RELEASE_STREAM ? 0x80000000
      : command == RECORD_RTMP_FC_PUBLISH ? 0x1000001 : 0xffffffff;
   c.stream = 0xffffffff;
   return c;
}

static void bounds(void)
{
   uint8_t output[1026];
   record_rtmp_command_config_t c;
   size_t size, needed, cap, i;
   unsigned command;
   for (command = RECORD_RTMP_CONNECT; command <= RECORD_RTMP_FC_PUBLISH; command++)
   {
      c = config(command);
      assert(record_rtmp_command_encode((enum record_rtmp_command)command,
               &c, golden, sizeof(golden), &needed));
      for (cap = 0; cap < needed; cap++)
      {
         memset(output, 0xa5, sizeof(output));
         size = 999;
         assert(!record_rtmp_command_encode((enum record_rtmp_command)command,
                  &c, output, cap, &size) && !size);
         for (i = cap; i < sizeof(output); i++)
            assert(output[i] == 0xa5);
      }
      assert(record_rtmp_command_encode((enum record_rtmp_command)command,
               &c, output, needed, &size) && size == needed);
      assert(!memcmp(output, golden, needed));
   }
   memset(oversized, 'a', sizeof(oversized) - 1);
   c = config(0);
   c.application = oversized;
   assert(!record_rtmp_command_encode(RECORD_RTMP_CONNECT, &c, output, sizeof(output), &size) && !size);
   c = config(0);
   c.url = NULL;
   assert(!record_rtmp_command_encode(RECORD_RTMP_CONNECT, &c, output, sizeof(output), &size));
   c = config(1);
   c.transaction = 0;
   assert(!record_rtmp_command_encode(RECORD_RTMP_CREATE_STREAM, &c, output, sizeof(output), &size));
   c = config(2);
   c.name = "";
   assert(!record_rtmp_command_encode(RECORD_RTMP_PUBLISH, &c, output, sizeof(output), &size));
   c = config(3);
   c.stream = 0;
   assert(!record_rtmp_command_encode(RECORD_RTMP_DELETE_STREAM, &c, output, sizeof(output), &size));
   assert(!record_rtmp_command_encode((enum record_rtmp_command)99, &c, output, sizeof(output), &size));
}

int main(int argc, char **argv)
{
   record_rtmp_command_config_t c;
   FILE *f;
   size_t size;
   unsigned command;
   uint8_t length[2];
   if (argc == 4 && !strcmp(argv[1], "--connect"))
   {
      c = config(RECORD_RTMP_CONNECT);
      c.url = argv[3];
      assert(record_rtmp_command_encode(RECORD_RTMP_CONNECT, &c, golden, sizeof(golden), &size));
      f = fopen(argv[2], "wb");
      assert(f && fwrite(golden, 1, size, f) == size && !fclose(f));
      return 0;
   }
   assert(argc == 1 || argc == 2);
   bounds();
   if (argc == 2)
   {
      f = fopen(argv[1], "wb");
      assert(f);
      for (command = 0; command <= RECORD_RTMP_FC_PUBLISH; command++)
      {
         c = config(command);
         assert(record_rtmp_command_encode((enum record_rtmp_command)command,
                  &c, golden, sizeof(golden), &size));
         length[0] = (uint8_t)(size >> 8);
         length[1] = (uint8_t)size;
         assert(fwrite(length, 1, 2, f) == 2 && fwrite(golden, 1, size, f) == size);
      }
      assert(!fclose(f));
   }
   puts("PASS: AMF command admission, exact capacity and output bounds");
   return 0;
}
