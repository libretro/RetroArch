/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef TLS_HANDSHAKE_DRIVER_H
#define TLS_HANDSHAKE_DRIVER_H
#include <features/features_cpu.h>

/* The fixture owns waits and its deadline; the TLS step owns neither. */
static int tls_test_connect(void *ssl, int fd, void *address, bool incremental)
{
   retro_time_t deadline;
   if (!incremental)
      return ssl_socket_connect(ssl, address, true, false);
   if (!socket_connect_with_timeout(fd, address, 5000))
      return -1;
   deadline = cpu_features_get_time_usec()
      + (retro_time_t)SSL_SOCKET_IO_TIMEOUT_MS * 1000;
   for (;;)
   {
      bool read = false, write = false;
      int result = ssl_socket_handshake_step(ssl, &read, &write);
      retro_time_t remaining = deadline - cpu_features_get_time_usec();
      if (result)
         return result;
      if (remaining <= 0)
         return -1;
      if (read || write)
      {
         int timeout = (int)((remaining + 999) / 1000);
         if (!socket_wait(fd, &read, &write, timeout))
            return -1;
      }
   }
}
#endif
