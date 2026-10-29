/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <net/net_rtmp_command.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t golden[1024];
static char oversized[4098];

static net_rtmp_command_config_t config(unsigned command)
{
   net_rtmp_command_config_t c;
   c.application = "live";
   c.url = "rtmp://127.0.0.1/live";
   c.name = "test/\303\251";
   c.transaction = command == NET_RTMP_RELEASE_STREAM ? 0x80000000
      : command == NET_RTMP_FC_PUBLISH ? 0x1000001 : 0xffffffff;
   c.stream = 0xffffffff;
   return c;
}

static void bounds(void)
{
   uint8_t output[1026];
   net_rtmp_command_config_t c;
   size_t size, needed, cap, i;
   unsigned command;
   for (command = NET_RTMP_CONNECT; command <= NET_RTMP_FC_PUBLISH; command++)
   {
      c = config(command);
      assert(net_rtmp_command_encode((enum net_rtmp_command)command,
               &c, golden, sizeof(golden), &needed));
      for (cap = 0; cap < needed; cap++)
      {
         memset(output, 0xa5, sizeof(output));
         size = 999;
         assert(!net_rtmp_command_encode((enum net_rtmp_command)command,
                  &c, output, cap, &size) && !size);
         for (i = cap; i < sizeof(output); i++)
            assert(output[i] == 0xa5);
      }
      assert(net_rtmp_command_encode((enum net_rtmp_command)command,
               &c, output, needed, &size) && size == needed);
      assert(!memcmp(output, golden, needed));
   }
   memset(oversized, 'a', sizeof(oversized) - 1);
   c = config(0);
   c.application = oversized;
   assert(!net_rtmp_command_encode(NET_RTMP_CONNECT, &c, output, sizeof(output), &size) && !size);
   c = config(0);
   c.url = NULL;
   assert(!net_rtmp_command_encode(NET_RTMP_CONNECT, &c, output, sizeof(output), &size));
   c = config(1);
   c.transaction = 0;
   assert(!net_rtmp_command_encode(NET_RTMP_CREATE_STREAM, &c, output, sizeof(output), &size));
   c = config(2);
   c.name = "";
   assert(!net_rtmp_command_encode(NET_RTMP_PUBLISH, &c, output, sizeof(output), &size));
   c = config(3);
   c.stream = 0;
   assert(!net_rtmp_command_encode(NET_RTMP_DELETE_STREAM, &c, output, sizeof(output), &size));
   assert(!net_rtmp_command_encode((enum net_rtmp_command)99, &c, output, sizeof(output), &size));
}

int main(int argc, char **argv)
{
   net_rtmp_command_config_t c;
   FILE *f;
   size_t size;
   unsigned command;
   uint8_t length[2];
   if (argc == 4 && !strcmp(argv[1], "--connect"))
   {
      c = config(NET_RTMP_CONNECT);
      c.url = argv[3];
      assert(net_rtmp_command_encode(NET_RTMP_CONNECT, &c, golden, sizeof(golden), &size));
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
      for (command = 0; command <= NET_RTMP_FC_PUBLISH; command++)
      {
         c = config(command);
         assert(net_rtmp_command_encode((enum net_rtmp_command)command,
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
