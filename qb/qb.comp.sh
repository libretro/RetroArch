. qb/config.comp.sh

TEMP_C=.tmp.c
TEMP_CXX=.tmp.cxx
TEMP_EXE=.tmp

ECHOBUF="Checking operating system"
#echo -n "Checking operating system"

if [ -n "$CROSS_COMPILE" ]; then
	case "$CROSS_COMPILE" in
		*'-mingw32'*) OS='Win32';;
		*);;
	esac
fi

if [ -z "$OS" ]; then
	case "$(uname)" in
		'Linux') OS='Linux';;
		*'BSD') OS='BSD';;
		'Darwin') OS='Darwin';;
		'MINGW32'*) OS='MinGW';;
		'CYGWIN'*) OS='Cygwin';;
		'Haiku') OS='Haiku';;
		*) OS="Win32";;
	esac
fi

echo "$ECHOBUF ... $OS"

# Checking for working C compiler
if [ "$USE_LANG_C" = 'yes' ]; then
	ECHOBUF="Checking for suitable working C compiler"
#	echo -n "Checking for suitable working C compiler"
	cat << EOF > "$TEMP_C"
#include <stdio.h>
int main(void) { puts("Hai world!"); return 0; }
EOF
	if [ -z "$CC" ]; then
		for CC in ${CC:=$(which ${CROSS_COMPILE}gcc ${CROSS_COMPILE}cc ${CROSS_COMPILE}clang)} ''; do
			"$CC" -o "$TEMP_EXE" "$TEMP_C" >/dev/null 2>&1 && break
		done
	fi
	[ "$CC" ] || { echo "$ECHOBUF ... Not found. Exiting."; exit 1;}
	echo "$ECHOBUF ... $CC"
	rm -f "$TEMP_C" "$TEMP_EXE"
fi

# Checking for working C++
if [ "$USE_LANG_CXX" = 'yes' ]; then
	ECHOBUF="Checking for suitable working C++ compiler"
#	echo -n "Checking for suitable working C++ compiler"
	cat << EOF > "$TEMP_CXX"
#include <iostream>
int main() { std::cout << "Hai guise" << std::endl; return 0; }
EOF
	if [ -z "$CXX" ]; then
		for CXX in ${CXX:=$(which ${CROSS_COMPILE}g++ ${CROSS_COMPILE}c++ ${CROSS_COMPILE}clang++)} ''; do
			"$CXX" -o "$TEMP_EXE" "$TEMP_CXX" >/dev/null 2>&1 && break
		done
	fi
	[ "$CXX" ] || { echo "$ECHOBUF ... Not found. Exiting."; exit 1;}
	echo "$ECHOBUF ... $CXX"
	rm -f "$TEMP_CXX" "$TEMP_EXE"
fi

if [ "$OS" = "Win32" ]; then
	ECHOBUF="Checking for windres"
	if [ -z "$WINDRES" ]; then
		WINDRES=$(which ${CROSS_COMPILE}windres)
		[ "$WINDRES" ] || { echo "$ECHOBUF ... Not found. Exiting."; exit 1; }
	fi
	echo "$ECHOBUF ... $WINDRES"
fi
