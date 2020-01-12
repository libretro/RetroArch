. qb/config.comp.sh

TEMP_C=.tmp.c
TEMP_CXX=.tmp.cxx
TEMP_EXE=.tmp

CC="${CC:-}"
CXX="${CXX:-}"
PKG_CONF_PATH="${PKG_CONF_PATH:-}"
WINDRES="${WINDRES:-}"

# Checking for working C compiler
cat << EOF > "$TEMP_C"
#include <stdio.h>
int main(void) { puts("Hai world!"); return 0; }
EOF

printf %s 'Checking for suitable working C compiler ... '

cc_works=0
add_opt CC no
if [ "$CC" ]; then
	"$CC" -o "$TEMP_EXE" "$TEMP_C" >/dev/null 2>&1 && cc_works=1
else
	for cc in gcc cc clang; do
		CC="$(exists "${CROSS_COMPILE}${cc}")" || CC=""
		if [ "$CC" ]; then
			"$CC" -o "$TEMP_EXE" "$TEMP_C" >/dev/null 2>&1 && {
				cc_works=1; break
			}
		fi
	done
fi

rm -f -- "$TEMP_C" "$TEMP_EXE"

cc_status='does not work'
if [ "$cc_works" = '1' ]; then
	cc_status='works'
	HAVE_CC='yes'
elif [ -z "$CC" ]; then
	cc_status='not found'
fi

printf %s\\n "$CC $cc_status"

if [ "$cc_works" = '0' ] && [ "$USE_LANG_C" = 'yes' ]; then
	die 1 'Error: Cannot proceed without a working C compiler.'
fi

# Checking for working C++
cat << EOF > "$TEMP_CXX"
#include <iostream>
int main() { std::cout << "Hai guise" << std::endl; return 0; }
EOF

printf %s 'Checking for suitable working C++ compiler ... '

cxx_works=0
add_opt CXX no
if [ "$CXX" ]; then
	"$CXX" -o "$TEMP_EXE" "$TEMP_CXX" >/dev/null 2>&1 && cxx_works=1
else
	for cxx in g++ c++ clang++; do
		CXX="$(exists "${CROSS_COMPILE}${cxx}")" || CXX=""
		if [ "$CXX" ]; then
			"$CXX" -o "$TEMP_EXE" "$TEMP_CXX" >/dev/null 2>&1 && {
				cxx_works=1; break
			}
		fi
	done
fi

rm -f -- "$TEMP_CXX" "$TEMP_EXE"

cxx_status='does not work'
if [ "$cxx_works" = '1' ]; then
	cxx_status='works'
	HAVE_CXX='yes'
elif [ -z "$CXX" ]; then
	cxx_status='not found'
fi

printf %s\\n "$CXX $cxx_status"

if [ "$cxx_works" = '0' ] && [ "$USE_LANG_CXX" = 'yes' ]; then
	die : 'Warning: A working C++ compiler was not found, C++ features will be disabled.'
fi

if [ "$OS" = "Win32" ]; then
	printf %s 'Checking for windres ... '

	WINDRES="${WINDRES:-$(exists "${CROSS_COMPILE}windres" || :)}"

	printf %s\\n "${WINDRES:=none}"

	if [ "$WINDRES" = none ]; then
		die 1 'Error: Cannot proceed without windres.'
	fi
fi

printf %s 'Checking for pkg-config ... '

PKG_CONF_PATH="${PKG_CONF_PATH:-$(exists "${CROSS_COMPILE}pkgconf" ||
	exists "${CROSS_COMPILE}pkg-config" || :)}"

printf %s\\n "${PKG_CONF_PATH:=none}"

if [ "$PKG_CONF_PATH" = "none" ]; then
	die : 'Warning: pkg-config not found, package checks will fail.'
fi
