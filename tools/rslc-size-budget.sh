#!/bin/sh
# Code size of the rslc compiler units against a checked-in budget.
#
# The compiler replaces 4.3 MB of glslang + SPIRV-Cross text; the
# point of the budget is to notice the dialect widening into a general
# compiler, which no test sees. The numbers are .text bytes of each
# unit compiled the way libretro-common ships it (C89, -O2), summed
# for the two build shapes: Vulkan-only (TU 1 + TU 2) and full (all
# three). Run by hand before delivering a change under
# libretro-common/gfx/shaders/. When a change means to grow a unit, the
# reference moves with it in the same commit, with the reason in the
# message.
#
#   tools/rslc-size-budget.sh          measure and compare
#   tools/rslc-size-budget.sh --write  rewrite the reference from this
#                                      host (reference host only)
set -eu

cd "$(dirname "$0")/.."
ref="tools/rslc-size-budget.ref"
inc="libretro-common/include"
units="rslc_front rslc_ir rslc_emit"
cc="${CC:-gcc}"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

measure() {
   "$cc" -std=c89 -O2 -Wall -I"$inc" -Ilibretro-common/gfx/shaders \
      -c "libretro-common/gfx/shaders/$1.c" -o "$tmp/$1.o"
   size "$tmp/$1.o" | awk 'NR == 2 { print $1 }'
}

front=$(measure rslc_front)
ir=$(measure rslc_ir)
emit=$(measure rslc_emit)
vk=$(( front + ir ))
full=$(( front + ir + emit ))

if [ "${1:-}" = "--write" ]; then
   {
      echo "# unit        .text bytes   (x86-64 GCC -O2 -std=c89)"
      printf 'rslc_front   %8d\n' "$front"
      printf 'rslc_ir      %8d\n' "$ir"
      printf 'rslc_emit    %8d\n' "$emit"
      echo "# ceilings, from the plan: Vulkan-only build and full build"
      echo "vulkan_only_max  153600"
      echo "full_max         460800"
   } > "$ref"
   cat "$ref"
   exit 0
fi

fail=0
for u in $units; do
   case $u in
      rslc_front) now=$front ;;
      rslc_ir)    now=$ir ;;
      rslc_emit)  now=$emit ;;
   esac
   want=$(awk -v f="$u" '$1 == f { print $2 }' "$ref")
   [ -n "$want" ] || { echo "SKIP  $u (no reference)"; continue; }
   max=$(( want + want / 4 ))
   if [ "$now" -gt "$max" ]; then
      echo "FAIL  $u: $now bytes, reference $want (max $max)"
      fail=1
   else
      echo "ok    $u: $now bytes (reference $want)"
   fi
done

vkmax=$(awk '$1 == "vulkan_only_max" { print $2 }' "$ref")
fullmax=$(awk '$1 == "full_max" { print $2 }' "$ref")
if [ "$vk" -gt "$vkmax" ]; then
   echo "FAIL  vulkan-only build: $vk bytes, ceiling $vkmax"; fail=1
else
   echo "ok    vulkan-only build: $vk bytes (ceiling $vkmax)"
fi
if [ "$full" -gt "$fullmax" ]; then
   echo "FAIL  full build: $full bytes, ceiling $fullmax"; fail=1
else
   echo "ok    full build: $full bytes (ceiling $fullmax)"
fi
exit $fail
