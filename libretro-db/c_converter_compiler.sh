#!/usr/bin/env bash

# Iterate through each dat file to compile.
for file in libretro-database/rdb/*.rdb; do
   # Find the name of the rdb file.
   name=$(basename --suffix=.rdb "$file")

   # Tell the user which database file we are acting on.
   echo -e "\033[1m$name\033[0m"

   # Find all related DAT files, and send them over to c_converter.
   find -name "${name}.dat" -print0 | sort -z | xargs -0 ./c_converter "${file}"
done
