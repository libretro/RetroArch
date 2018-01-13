#!/bin/bash

#
# This script listens for the WiiU network logger and prints the messages to
# the terminal.
#
# If you would like a logfile, pipe this script's output to tee.

script_dir=$(dirname $(readlink -f $0))

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

#
# This prevents a tug-of-war between bash and netcat as to who gets the
# CTRL+C code.
#
trap 'exit_listen_loop=1' SIGINT

while [ $exit_listen_loop -eq 0 ]; do
  echo ========= `date` =========
  netcat -p $PC_DEVELOPMENT_TCP_PORT -l
done
