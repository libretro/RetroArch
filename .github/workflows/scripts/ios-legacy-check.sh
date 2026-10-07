#!/bin/sh
# The iOS 6 build, compiled for armv7 the way RetroArch_iOS6.xcodeproj
# builds it: its own defines (read from the project, so this follows
# it), ARC, gnu99, against an iOS SDK with a 6.0 deployment target.
#
# Everything newer than iOS 6 has to sit behind a runtime check -
# apple_runtime_available() and a selector send, or an availability-
# annotated method - and clang's -Wunguarded-availability is what
# enforces that, so every warning is an error here. An API the SDK does
# not declare at all is an error too: the iOS 6 build reaches newer
# APIs by selector, not by needing a newer SDK.
#
# The SDK is the iOS 9.3 one - Xcode 7's, the last to take a 6.0
# deployment target - and only its headers are needed: this compiles,
# it does not link.
#
# usage: ios-legacy-check.sh <path to iPhoneOS9.3.sdk> [floor]
#   floor defaults to 6.0
#
# Needs clang (any version that targets armv7-apple-ios).

SDK=$1
FLOOR=${2:-6.0}
CC=${CC:-clang}
PROJECT=pkg/apple/RetroArch_iOS6.xcodeproj/project.pbxproj

if [ -z "$SDK" ] || [ ! -d "$SDK/System/Library/Frameworks/UIKit.framework" ]; then
   echo "usage: $0 <path to iPhoneOS9.3.sdk> [floor]" >&2
   exit 2
fi
if [ ! -f "$PROJECT" ]; then
   echo "run from the top of the RetroArch tree" >&2
   exit 2
fi

# The project's OTHER_CFLAGS defines, each once
DEFS=$(grep -o '"-D[A-Za-z_0-9=]*"' "$PROJECT" | tr -d '"' | sort -u | tr '\n' ' ')

# Xcode finds the project's own headers by bare name through its
# header map; every directory under pkg/apple and ui/drivers that
# holds one stands in for it.
HMAP=$(find pkg/apple ui/drivers -name '*.h' -exec dirname {} \; | sort -u | sed 's/^/-I/' | tr '\n' ' ')

INC="-Ideps -Ideps/7zip -Ideps/rcheevos/include -Ideps/stb -I. -Igfx/include -Ilibretro-common/include"

FLAGS="-fsyntax-only -target armv7-apple-ios$FLOOR -isysroot $SDK -std=gnu99 -fblocks
   -Wunguarded-availability -Wunguarded-availability-new -Wdeprecated-declarations
   -Wno-nullability-completeness -Werror -ferror-limit=0"

fail=0
check() {
   name=$1; lang=$2; tu=$3; shift 3
   # shellcheck disable=SC2086
   if out=$($CC -x "$lang" $FLAGS "$@" $DEFS $INC $HMAP "$tu" 2>&1); then
      echo "ok    $name"
   else
      echo "FAIL  $name"
      printf '%s\n' "$out" | sed 's/^/      /'
      fail=1
   fi
}

echo "== iOS $FLOOR, armv7, $(basename "$SDK") =="
check "griffin_objc.m (ARC)" objective-c griffin/griffin_objc.m -fobjc-arc
check "griffin.c"            c           griffin/griffin.c

exit $fail
