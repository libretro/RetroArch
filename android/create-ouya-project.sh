#!/bin/sh

rm -r phoenix-ouya
cp -r phoenix phoenix-ouya
mv phoenix-ouya/src/org phoenix-ouya/src/com
rm -r phoenix-ouya/gen
find phoenix-ouya -type f -print0 | xargs -0 sed -b -i 's/org.retroarch/com.retroarch/g'
