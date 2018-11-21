#!/bin/bash

#
# This script listens for the WiiU network logger and prints the messages to
# the terminal.
#
# If you would like a logfile, pipe this script's output to tee.

NETCAT=

find_netcat()
{
  NETCAT=$(which netcat 2>/dev/null |grep '/')
  if [ -z "$NETCAT" ]; then
    NETCAT=$(which nc 2>/dev/null |grep '/')
    if [ -z "$NETCAT" ]; then
      echo "Failed to find either 'netcat' or 'nc'. Please install it."
      exit 1
    fi
  fi
}

do_readlink()
{
  local exe=$1
  echo "$(cd $(dirname $exe) && pwd)"
}

find_netcat
script_dir=$(do_readlink $0)

IP=$(which ip 2>/dev/null | grep '^/')
IFCONFIG=$(which ifconfig 2>/dev/null | grep '^/')
TS=$(which ts 2>/dev/null | grep '^/')

# Using wiiu-devel.properties ensure your make file and this listen script
# stay in sync with each other.
#
# See wiiu-devel.properties.template for instructions.

if [ -e "$script_dir/../wiiu-devel.properties" ]; then
  . $script_dir/../wiiu-devel.properties
fi

if [ -z "$PC_DEVELOPMENT_TCP_PORT" ]; then
  PC_DEVELOPMENT_TCP_PORT=4405
fi

exit_listen_loop=0

getBroadcastIp()
{
  if [ ! -z "$IP" ]; then
    $IP addr show | grep 'inet' |grep 'brd' | awk '{print $4}'
  elif [ ! -z "$IFCONFIG" ]; then
    $IFCONFIG | grep 'broadcast' | awk '{print $6}'
  else
    echo "255.255.255.255"
  fi
}

#
# This prevents a tug-of-war between bash and netcat as to who gets the
# CTRL+C code.
#
trap 'exit_listen_loop=1' SIGINT

if [ -z "$TS" ]; then
  echo "[WARN] 'ts' not found. Install the moreutils package to get timestamps."
fi

broadcast=$(getBroadcastIp)
echo "Listening for UDP packets on broadcast IP: $broadcast"

while [ $exit_listen_loop -eq 0 ]; do
  echo ========= `date` =========
  if [ -z "$TS" ]; then
    $NETCAT -kluw 0 $broadcast $PC_DEVELOPMENT_TCP_PORT
  else
    $NETCAT -kluw 0 $broadcast $PC_DEVELOPMENT_TCP_PORT |ts '[%Y-%m-%d %H:%M:%.S]'
  fi
done
