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
	for CC in $(exists ${CROSS_COMPILE}gcc ${CROSS_COMPILE}cc ${CROSS_COMPILE}clang) ''; do
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
	die 1 'Error: Cannot proceed without a working C compiler.'
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
	for CXX in $(exists ${CROSS_COMPILE}g++ ${CROSS_COMPILE}c++ ${CROSS_COMPILE}clang++) ''; do
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
	die 1 'Error: Cannot proceed without a working C++ compiler.'
fi

if [ "$OS" = "Win32" ]; then
	echobuf="Checking for windres"
	if [ -z "$WINDRES" ]; then
		WINDRES=$(exists ${CROSS_COMPILE}windres)
		[ "$WINDRES" ] || die 1 "$echobuf ... Not found. Exiting."
	fi
	echo "$echobuf ... $WINDRES"
fi

[ -n "$PKG_CONF_PATH" ] || {
	PKG_CONF_PATH="none"

	for p in $(exists "${CROSS_COMPILE}pkg-config") ''; do
		[ -n "$p" ] && {
			PKG_CONF_PATH=$p;
			break;
		}
	done

}

echo "Checking for pkg-config ... $PKG_CONF_PATH"

if [ "$PKG_CONF_PATH" = "none" ]; then
	die : 'Warning: pkg-config not found, package checks will fail.'
fi
