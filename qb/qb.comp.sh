. qb/config.comp.sh

TEMP_C=.tmp.c
TEMP_CXX=.tmp.cxx
TEMP_MOC=.moc.h
TEMP_CPP=.moc.cpp
TEMP_EXE=.tmp

# Checking for working C compiler
cat << EOF > "$TEMP_C"
#include <stdio.h>
int main(void) { puts("Hai world!"); return 0; }
EOF

cc_works=0
HAVE_CC=no
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
HAVE_CXX=no
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

echo "Checking for suitable working C++ compiler ... $CXX $cxx_status"

if [ "$cxx_works" = '0' ] && [ "$USE_LANG_CXX" = 'yes' ]; then
	die : 'Warning: A working C++ compiler was not found, C++ features will be disabled.'
fi

if [ "$OS" = "Win32" ]; then
	echobuf="Checking for windres"
	if [ -z "$WINDRES" ]; then
		WINDRES="$(exists "${CROSS_COMPILE}windres")" || WINDRES=""
		[ -z "$WINDRES" ] && die 1 "$echobuf ... Not found. Exiting."
	fi
	echo "$echobuf ... $WINDRES"
fi

if [ -z "$PKG_CONF_PATH" ]; then
	PKG_CONF_PATH="none"
	for pkgconf in pkgconf pkg-config; do
		PKGCONF="$(exists "${CROSS_COMPILE}${pkgconf}")" || PKGCONF=""
		[ "$PKGCONF" ] && {
			PKG_CONF_PATH="$PKGCONF"
			break
		}
	done
fi

echo "Checking for pkg-config ... $PKG_CONF_PATH"

if [ "$PKG_CONF_PATH" = "none" ]; then
	die : 'Warning: pkg-config not found, package checks will fail.'
fi

# Checking for working moc
cat << EOF > "$TEMP_MOC"
#include <QTimeZone>
class Test : public QObject
{
public:
   Q_OBJECT
   QTimeZone tz;
};
EOF

HAVE_MOC=no
if [ "$HAVE_QT" != "no" ] && [ "$HAVE_CXX" != "no" ] && [ "$PKG_CONF_PATH" != "none" ]; then
	moc_works=0
	if "$PKGCONF" --exists Qt5Core; then
		if [ "$MOC" ]; then
			"$MOC" -o "$TEMP_CPP" "$TEMP_MOC" >/dev/null 2>&1 &&
			"$CXX" -o "$TEMP_EXE" $("$PKGCONF" --cflags --libs Qt5Core) -fPIC \
				-c "$TEMP_CPP" >/dev/null 2>&1 &&
			moc_works=1
		else
			for moc in moc-qt5 moc; do
				MOC="$(exists "$moc")" || MOC=""
				if [ "$MOC" ]; then
					"$MOC" -o "$TEMP_CPP" "$TEMP_MOC" >/dev/null 2>&1 ||
						continue
					"$CXX" -o "$TEMP_EXE" $("$PKGCONF" --cflags --libs Qt5Core) \
						-fPIC -c "$TEMP_CPP" >/dev/null 2>&1 && {
						moc_works=1
						break
					}
				fi
			done
		fi
	else
		MOC=""
	fi

	moc_status='does not work'
	if [ "$moc_works" = '1' ]; then
		moc_status='works'
		HAVE_MOC='yes'
	elif [ -z "$MOC" ]; then
		moc_status='not found'
	fi

	echo "Checking for moc ... $MOC $moc_status"

	if [ "$HAVE_MOC" != 'yes' ]; then
		die : 'Warning: moc not found, Qt companion support will be disabled.'
	fi
fi

rm -f -- "$TEMP_CPP" "$TEMP_EXE" "$TEMP_MOC"
