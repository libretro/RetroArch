#!/bin/bash
set -x

DEST=${1:-../pkg/apple/iOS/Resources/Media.xcassets/AppIcon.appiconset}

for pt in 29 40 57 60 50 72 76 ; do
    for scale in 1 2 3 ; do
        side=$(expr $pt \* $scale)
        convert retroarch.svg -resize ${side}x${side} -negate ${DEST}/Icon-${pt}-${scale}.png
    done
done
