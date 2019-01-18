#!/bin/sh

source "$(dirname $0)/env-vars.sh"

exec retroarch-phoenix "${@}"
