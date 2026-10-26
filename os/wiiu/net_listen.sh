#!/usr/bin/env python
# Adapted from the Python UDP broadcast client example by ninedraft: https://github.com/ninedraft/python-udp/

from __future__ import print_function

import datetime
import os
import signal
import sys

from socket import (
    socket,
    AF_INET,
    SOCK_DGRAM,
    IPPROTO_UDP,
    SOL_SOCKET,
    SO_REUSEPORT,
    SO_BROADCAST
)

def get_bind_port():
  default = 4405
  candidate = os.getenv('PC_DEVELOPMENT_TCP_PORT', str(default))
  try:
    port = int(candidate)
  except ValueError:
    port = default

  return port

def exit_gracefully(signum, frame):
    sys.exit(0)


def main():
    signal.signal(signal.SIGINT, exit_gracefully)
    signal.signal(signal.SIGTERM, exit_gracefully)

    client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
    client.setsockopt(SOL_SOCKET, SO_REUSEPORT, 1)
    client.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
    client.bind(("", get_bind_port()) )
    while True:
      data, addr = client.recvfrom(1024)
      print(datetime.datetime.now().isoformat(), data.decode('utf-8').rstrip('\r\n'))


if __name__ == '__main__':
    main()
