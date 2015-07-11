. qb/config.comp.sh

TEMP_C=.tmp.c
TEMP_CXX=.tmp.cxx
TEMP_EXE=.tmp

# Checking for working C compiler
cat << EOF > "$TEMP_C"
#include <stdio.h>
int main(void) { puts("Hai world!"); return 0; }
EOF

cc_works=0
if [ "$CC" ]; then
	"$CC" -o "$TEMP_EXE" "$TEMP_C" >/dev/null 2>&1 && cc_works=1
else
	for CC in ${CC:=$(which ${CROSS_COMPILE}gcc ${CROSS_COMPILE}cc ${CROSS_COMPILE}clang 2>/dev/null)} ''; do
		"$CC" -o "$TEMP_EXE" "$TEMP_C" >/dev/null 2>&1 && cc_works=1 && break
	done
fi

rm -f "$TEMP_C" "$TEMP_EXE"

cc_status='does not work'
if [ "$cc_works" = '1' ]; then
	cc_status='works'
elif [ -z "$CC" ]; then
	cc_status='not found'
fi

echo "Checking for suitable working C compiler ... $CC $cc_status"

if [ "$cc_works" = '0' ] && [ "$USE_LANG_C" = 'yes' ]; then
	echo "Error: Cannot proceed without a working C compiler."
	exit 1
fi

# Checking for working C++
cat << EOF > "$TEMP_CXX"
#include <iostream>
int main() { std::cout << "Hai guise" << std::endl; return 0; }
EOF

cxx_works=0
if [ "$CXX" ]; then
	"$CXX" -o "$TEMP_EXE" "$TEMP_CXX" >/dev/null 2>&1 && cxx_works=1
else
	for CXX in ${CXX:=$(which ${CROSS_COMPILE}g++ ${CROSS_COMPILE}c++ ${CROSS_COMPILE}clang++ 2>/dev/null)} ''; do
		"$CXX" -o "$TEMP_EXE" "$TEMP_CXX" >/dev/null 2>&1 && cxx_works=1 && break
	done
fi

rm -f "$TEMP_CXX" "$TEMP_EXE"

cxx_status='does not work'
if [ "$cxx_works" = '1' ]; then
	cxx_status='works'
elif [ -z "$CXX" ]; then
	cxx_status='not found'
fi

echo "Checking for suitable working C++ compiler ... $CXX $cxx_status"

if [ "$cxx_works" = '0' ] && [ "$USE_LANG_CXX" = 'yes' ]; then
	echo "Error: Cannot proceed without a working C++ compiler."
	exit 1
fi

if [ "$OS" = "Win32" ]; then
	echobuf="Checking for windres"
	if [ -z "$WINDRES" ]; then
		WINDRES=$(which ${CROSS_COMPILE}windres)
		[ "$WINDRES" ] || { echo "$echobuf ... Not found. Exiting."; exit 1; }
	fi
	echo "$echobuf ... $WINDRES"
fi

[ -n "$PKG_CONF_PATH" ] || {
	PKG_CONF_PATH="none"

	for path in $(which "${CROSS_COMPILE}pkg-config" 2>/dev/null) ''; do
		[ -n "$path" ] && {
			PKG_CONF_PATH=$path;
			break;
		}
	done

}

echo "Checking for pkg-config ... $PKG_CONF_PATH"

if [ "$PKG_CONF_PATH" = "none" ]; then
	echo "Warning: pkg-config not found, package checks will fail."
fi
