#!/bin/sh
# Exercise real decoder ownership and quit draining for each music container.
# Requires a built exit_drain_test with the corresponding codecs enabled.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
fixtures=$(mktemp -d)
trap 'rm -rf "$fixtures"' EXIT HUP INT TERM
for spec in flac:flac:flac wav:pcm_s16le:wav mp3:libmp3lame:mp3 \
      ogg:libvorbis:ogg opus:libopus:opus aac:aac:adts m4a:aac:mp4 \
      opus-weba:libopus:webm vorbis-weba:libvorbis:webm \
      wma:wmav2:asf xwma:wmav2:wav; do
   name=${spec%%:*}
   rest=${spec#*:}
   codec=${rest%%:*}
   mux=${rest#*:}
   ext=$name
   case "$name" in *-weba) ext=weba ;; esac
   file="$fixtures/$name.$ext"
   ffmpeg -y -v error -f lavfi -i sine=frequency=440:sample_rate=48000 \
      -t 5 -c:a "$codec" -f "$mux" "$file"
   # FFmpeg has no XWMA muxer; use its WMA-in-RIFF packets with the
   # XWMA form identifier understood by the native decoder.
   if [ "$name" = xwma ]; then
      printf XWMA | dd of="$file" bs=1 seek=8 conv=notrunc 2>/dev/null
   fi
   echo "Shutdown regression: $name"
   expected=
   case "$name" in flac|wav|m4a|opus) expected=5000000 ;; esac
   if [ -n "$expected" ]; then export MUSIC_DURATION_US=$expected;
   else unset MUSIC_DURATION_US; fi
   MUSIC_FIXTURE="$file" timeout 20 "$here/exit_drain_test"
done
