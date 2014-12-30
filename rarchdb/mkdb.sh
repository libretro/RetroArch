#!/bin/sh
./dat_converter "$1" "$2" && ./rarchdb_tool "$2" create-index crc crc && ./rarchdb_tool "$2" create-index md5 md5 && ./rarchdb_tool "$2" create-index sha1 sha1

