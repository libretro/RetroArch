/*
 * Copyright (c) 2017 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "compat/getopt.h"
#include "net/net_socket.h"

/* Only for #defines */
#include "../../network/netplay/netplay_private.h"

/* Macros to handle basic I/O */
#define ERROR() do { \
   fprintf(stderr, "Netplay disconnected.\n"); \
   exit(0); \
} while(0)

#define EXPAND() do { \
   while (cmd_size > payload_size) \
   { \
      payload_size *= 2; \
      payload = realloc(payload, payload_size); \
      if (!payload) \
      { \
         perror("realloc"); \
         exit(1); \
      } \
   } \
} while (0)

#define RECV() do { \
   if (!socket_receive_all_blocking(sock, &cmd, sizeof(uint32_t)) || \
       !socket_receive_all_blocking(sock, &cmd_size, sizeof(uint32_t))) \
      ERROR(); \
   cmd = ntohl(cmd); \
   cmd_size = ntohl(cmd_size); \
   EXPAND(); \
   if (!socket_receive_all_blocking(sock, payload, cmd_size)) \
      ERROR(); \
} while(0)

#define SEND() do { \
   uint32_t adj_cmd[2]; \
   adj_cmd[0] = htonl(cmd); \
   adj_cmd[1] = htonl(cmd_size); \
   if (!socket_send_all_blocking(sock, adj_cmd, sizeof(adj_cmd), true) || \
       !socket_send_all_blocking(sock, payload, cmd_size, true)) \
      ERROR(); \
} while(0)

#define WRITE() do { \
   uint32_t adj_cmd[2]; \
   adj_cmd[0] = htonl(cmd); \
   adj_cmd[1] = htonl(cmd_size); \
   if (write(ranp_out, adj_cmd, sizeof(adj_cmd)) != sizeof(adj_cmd) || \
       write(ranp_out, payload, cmd_size) != cmd_size) \
   { \
      perror(ranp_out_file_name); \
      close(ranp_out); \
      recording_started = recording = false; \
      if (!playing) \
         socket_close(sock); \
   } \
} while(0)

/* Our fds */
static int sock, ranp_in, ranp_out;

/* Space for netplay packets */
static uint32_t cmd, cmd_size, *payload;
static size_t payload_size;

/* The frame number when we connected, to offset frames being played or
 * recorded */
static uint32_t frame_offset = 0;

/* Usage statement */
void usage()
{
   fprintf(stderr,
      "Use: ranetplayer [options] [ranp file]\n"
      "Options:\n"
      "    -H|--host <address>:  Netplay host. Defaults to localhost.\n"
      "    -P|--port <port>:     Netplay port. Defaults to 55435.\n"
      "    -p|--play <file>:     Play back a recording over netplay.\n"
      "    -r|--record <file>:   Record netplay to a file.\n"
      "    -a|--ahead <frames>:  Number of frames by which to play ahead of the\n"
      "                          server. Tests rewind if negative, catch-up if\n"
      "                          positive.\n"
      "\n");
}

/* Offset the frame in a network packet to or from network time */
uint32_t frame_offset_cmd(bool ntoh)
{
   uint32_t frame = 0;

   switch (cmd)
   {
      case NETPLAY_CMD_INPUT:
      case NETPLAY_CMD_NOINPUT:
      case NETPLAY_CMD_MODE:
      case NETPLAY_CMD_CRC:
      case NETPLAY_CMD_LOAD_SAVESTATE:
      case NETPLAY_CMD_RESET:
         frame = ntohl(payload[0]);
         if (ntoh)
            frame -= frame_offset;
         else
            frame += frame_offset;
         payload[0] = htonl(frame);
         break;
   }

   return frame;
}

/* Send a bit of our input */
bool send_input(uint32_t cur_frame)
{
   while (1)
   {
      uint32_t rd_frame = 0;

      if (read(ranp_in, &cmd, sizeof(uint32_t)) != sizeof(uint32_t) ||
          read(ranp_in, &cmd_size, sizeof(uint32_t)) != sizeof(uint32_t))
         return false;

      cmd = ntohl(cmd);
      cmd_size = ntohl(cmd_size);
      EXPAND();

      if (read(ranp_in, payload, cmd_size) != cmd_size)
         return false;

      /* INFO is just saved for verification */
      if (cmd == NETPLAY_CMD_INFO)
         continue;

      /* Adjust the frame for commands we know */
      rd_frame = frame_offset_cmd(false);
      if (rd_frame)
         rd_frame -= frame_offset;

      /* And send it */
      SEND();

      if (rd_frame > cur_frame)
         break;
   }

   return true;
}

int main(int argc, char **argv)
{
   struct addrinfo *addr;
   uint32_t rd_frame = 0;
   int ahead = 0;
   const char *host = "localhost",
      *ranp_in_file_name = NULL,
      *ranp_out_file_name = NULL;
   int port = RARCH_DEFAULT_PORT;
   bool playing = false, playing_started = false,
      recording = false, recording_started = false;
   const char *optstring = NULL;

   const struct option opt[] = {
      {"host",       1, NULL, 'H'},
      {"port",       1, NULL, 'P'},
      {"play",       1, NULL, 'p'},
      {"record",     1, NULL, 'r'},
      {"ahead",      1, NULL, 'a'}
   };

   while (1)
   {
      int c;

      c = getopt_long(argc, argv, "H:P:p:r:a:", opt, NULL);
      if (c == -1)
         break;

      switch (c)
      {
         case 'H':
            host = optarg;
            break;

         case 'P':
            port = atoi(optarg);
            break;

         case 'p':
            playing = true;
            ranp_in_file_name = optarg;
            break;

         case 'r':
            recording = true;
            ranp_out_file_name = optarg;
            break;

         case 'a':
            ahead = atoi(optarg);
            break;

         default:
            usage();
            return 1;
      }
   }

   if (!playing && optind < argc)
   {
      playing = true;
      ranp_in_file_name = argv[optind++];
   }
   if (!playing && !recording)
   {
      usage();
      return 1;
   }

   /* Allocate space for the protocol */
   payload_size = 4096;
   payload = malloc(payload_size);
   if (!payload)
   {
      perror("malloc");
      return 1;
   }

   /* Open the input file, if applicable */
   if (playing)
   {
      ranp_in = open(ranp_in_file_name, O_RDONLY);
      if (ranp_in == -1)
      {
         perror(ranp_in_file_name);
         return 1;
      }
   }

   /* Open the output file, if applicable */
   if (recording)
   {
      ranp_out = open(ranp_out_file_name, O_WRONLY|O_CREAT|O_EXCL, 0666);
      if (ranp_out == -1)
      {
         perror(ranp_out_file_name);
         return 1;
      }
   }

   /* Connect to the netplay server */
   if ((sock = socket_init((void **) &addr, port, host, SOCKET_PROTOCOL_TCP)) < 0)
   {
      perror("socket");
      return 1;
   }

   if (socket_connect(sock, addr, false) < 0)
   {
      perror("connect");
      return 1;
   }

   /* Expect the header */
   if (!socket_receive_all_blocking(sock, payload, 6*sizeof(uint32_t)))
   {
      fprintf(stderr, "Failed to receive connection header.\n");
      return 1;
   }

   /* If it needs a password, too bad! */
   if (payload[3])
   {
      fprintf(stderr, "Password required but unsupported.\n");
      return 1;
   }

   /* Echo the connection header back */
   socket_send_all_blocking(sock, payload, 6*sizeof(uint32_t), true);

   /* Send a nickname */
   cmd = NETPLAY_CMD_NICK;
   cmd_size = 32;
   strcpy((char *) payload, "RANetplayer");
   SEND();

   /* Receive (and ignore) the nickname */
   RECV();

   /* Receive INFO */
   RECV();
   if (cmd != NETPLAY_CMD_INFO)
   {
      fprintf(stderr, "Failed to receive INFO.");
      return 1;
   }

   /* Save the INFO */
   if (recording)
      WRITE();

   /* Echo the INFO */
   SEND();

   /* Receive SYNC */
   RECV();

   /* If we're recording and NOT playing, we start immediately in spectator
    * mode */
   if (recording && !playing)
   {
      recording_started = true;
      frame_offset = ntohl(payload[0]);
   }

   /* If we're playing, request to enter PLAY mode */
   if (playing)
   {
      cmd = NETPLAY_CMD_PLAY;
      cmd_size = sizeof(uint32_t);
      payload[0] = htonl(1);
      SEND();
   }

   /* Now handle netplay commands */
   while (1)
   {
      RECV();

      frame_offset_cmd(true);

      /* Record this command */
      if (recording && recording_started)
         WRITE();

      /* Now handle it for sync and playback */
      switch (cmd)
      {
         case NETPLAY_CMD_MODE:
            if (playing && !playing_started)
            {
               uint32_t player;

               if (cmd_size < 2*sizeof(uint32_t)) break;

               /* See if this is us joining */
               player = ntohl(payload[1]);
               if ((player & NETPLAY_CMD_MODE_BIT_PLAYING) &&
                   (player & NETPLAY_CMD_MODE_BIT_YOU))
               {
                  /* This is where we start! */
                  playing_started = true;
                  frame_offset_cmd(false);
                  frame_offset = ntohl(payload[0]);

                  if (recording)
                     recording_started = true;

                  /* Send our current input */
                  send_input(0);
               }
            }
            break;

         case NETPLAY_CMD_INPUT:
         case NETPLAY_CMD_NOINPUT:
         {
            uint32_t frame;

            if (!playing || !playing_started) break;
            if (cmd_size < sizeof(uint32_t)) break;

            frame = ntohl(payload[0]);

            /* Only sync based on server time */
            if (cmd == NETPLAY_CMD_INPUT &&
                (cmd_size < 2*sizeof(uint32_t) ||
                 (ntohl(payload[1]) != 0)))
            {
               break;
            }

            if (frame > rd_frame)
            {
               rd_frame = frame;
               if (ahead >= 0 || frame >= (uint32_t) -ahead)
               {
                  if (!send_input(frame + ahead))
                  {
                     if (!recording)
                        socket_close(sock);
                     playing = playing_started = false;
                  }
               }
            }

            break;
         }
      }
   }

   return 0;
}
