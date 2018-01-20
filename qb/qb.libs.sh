MAKEFILE_DEFINES=''
CONFIG_DEFINES=''

[ "$PREFIX" ] || PREFIX="/usr/local"

add_define() # $1 = MAKEFILE or CONFIG $2 = define $3 = value
{ eval "${1}_DEFINES=\"\${${1}_DEFINES} $2=$3\""; }

add_dirs() # $1 = INCLUDE or LIBRARY  $@ = include or library paths
{	ADD="$1"; LINK="${1%"${1#?}"}"; shift
	while [ "$1" ]; do
		eval "${ADD}_DIRS=\"\${${ADD}_DIRS} -${LINK}${1}\""
		shift
	done
	eval "${ADD}_DIRS=\"\${${ADD}_DIRS# }\""
}

check_compiler() # $1 = language  $2 = function in lib
{	if [ "$1" = cxx ]; then
		COMPILER="$CXX"
		TEMP_CODE="$TEMP_CXX"
		TEST_C="extern \"C\" { void $2(void); } int main() { $2(); }"
	else
		COMPILER="$CC"
		TEMP_CODE="$TEMP_C"
		TEST_C="void $2(void); int main(void) { $2(); return 0; }"
	fi
}

check_lib() # $1 = language  $2 = HAVE_$2  $3 = lib  $4 = function in lib  $5 = extralibs $6 = headers $7 = critical error message [checked only if non-empty]
{	tmpval="$(eval "printf %s \"\$HAVE_$2\"")"
	[ "$tmpval" = 'no' ] && return 0

	check_compiler "$1" "$4"

	if [ "$4" ]; then
		ECHOBUF="Checking function $4 in ${3% }"
		if [ "$6" ]; then
			printf %s\\n "$6" "int main(void) { void *p = (void*)$4; return 0; }" > "$TEMP_CODE"
		else
			printf %s\\n "$TEST_C" > "$TEMP_CODE"
		fi
	else
		ECHOBUF="Checking existence of ${3% }"
		printf %s\\n 'int main(void) { return 0; }' > "$TEMP_CODE"
	fi

	val="$2"
	lib="$3"
	error="${7:-}"
	answer='no'
	eval "set -- $INCLUDE_DIRS $LIBRARY_DIRS $5 $CFLAGS $LDFLAGS $3"
	"$COMPILER" -o "$TEMP_EXE" "$TEMP_CODE" "$@" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$ECHOBUF ... $answer"
	rm -f -- "$TEMP_CODE" "$TEMP_EXE"

	if [ "$answer" = 'no' ]; then
		[ "$error" ] && die 1 "$error"
		[ "$tmpval" = 'yes' ] && {
			die 1 "Forced to build with library $lib, but cannot locate. Exiting ..."
		}
	else
		eval "${val}_LIBS=\"$lib\""
		PKG_CONF_USED="$PKG_CONF_USED $val"
	fi

	return 0
}

check_pkgconf() # $1 = HAVE_$1  $2 = package  $3 = version  $4 = critical error message [checked only if non-empty]
{	tmpval="$(eval "printf %s \"\$HAVE_$1\"")"
	eval "TMP_$1=\$tmpval"
	[ "$tmpval" = 'no' ] && return 0

	ECHOBUF="Checking presence of package $2"
	[ "$3" ] && ECHOBUF="$ECHOBUF >= $3"

	[ "$PKG_CONF_PATH" = "none" ] && {
		eval "HAVE_$1=no"
		printf %s\\n "$ECHOBUF ... no"
		return 0
	}

	answer='no'
	version='no'
	$PKG_CONF_PATH --atleast-version="${3:-0.0}" "$2" && {
		answer='yes'
		version="$("$PKG_CONF_PATH" --modversion "$2")"
		eval "$1_CFLAGS=\"$("$PKG_CONF_PATH" "$2" --cflags)\""
		eval "$1_LIBS=\"$("$PKG_CONF_PATH" "$2" --libs)\""
	}
	
	eval "HAVE_$1=\"$answer\""
	printf %s\\n "$ECHOBUF ... $version"
	if [ "$answer" = 'no' ]; then
		[ "$4" ] && die 1 "$4"
		[ "$tmpval" = 'yes' ] && \
			die 1 "Forced to build with package $2, but cannot locate. Exiting ..."
	else
		PKG_CONF_USED="$PKG_CONF_USED $1"
	fi
}

check_header() #$1 = HAVE_$1  $2..$5 = header files
{	tmpval="$(eval "printf %s \"\$HAVE_$1\"")"
	[ "$tmpval" = 'no' ] && return 0
	CHECKHEADER="$2"
	printf %s\\n "#include <$2>" > "$TEMP_C"
	[ "$3" != '' ] && CHECKHEADER="$3" && printf %s\\n "#include <$3>" >> "$TEMP_C"
	[ "$4" != '' ] && CHECKHEADER="$4" && printf %s\\n "#include <$4>" >> "$TEMP_C"
	[ "$5" != '' ] && CHECKHEADER="$5" && printf %s\\n "#include <$5>" >> "$TEMP_C"
	printf %s\\n "int main(void) { return 0; }" >> "$TEMP_C"
	answer='no'
	val="$1"
	header="$2"
	eval "set -- $INCLUDE_DIRS"
	"$CC" -o "$TEMP_EXE" "$TEMP_C" "$@" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "Checking presence of header file $CHECKHEADER ... $answer"
	rm -f -- "$TEMP_C" "$TEMP_EXE"
	[ "$tmpval" = 'yes' ] && [ "$answer" = 'no' ] && \
		die 1 "Build assumed that $header exists, but cannot locate. Exiting ..."
}

check_macro() #$1 = HAVE_$1  $2 = macro name
{	tmpval="$(eval "printf %s \"\$HAVE_$1\"")"
	[ "$tmpval" = 'no' ] && return 0
	ECHOBUF="Checking presence of predefined macro $2"
	cat << EOF > "$TEMP_C"
#ifndef $2
#error $2 is not defined
#endif
int main(void) { return 0; }
EOF
	answer='no'
	val="$1"
	macro="$2"
	eval "set -- $CFLAGS $INCLUDE_DIRS"
	"$CC" -o "$TEMP_EXE" "$TEMP_C" "$@" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$ECHOBUF ... $answer"
	rm -f -- "$TEMP_C" "$TEMP_EXE"
	[ "$tmpval" = 'yes' ] && [ "$answer" = 'no' ] && \
		die 1 "Build assumed that $macro is defined, but it's not. Exiting ..."
}

check_switch() # $1 = language  $2 = HAVE_$2  $3 = switch  $4 = critical error message [checked only if non-empty]
{	check_compiler "$1" ''

	ECHOBUF="Checking for availability of switch $3 in $COMPILER"
	printf %s\\n 'int main(void) { return 0; }' > "$TEMP_CODE"
	answer='no'
	"$COMPILER" -o "$TEMP_EXE" "$TEMP_CODE" "$3" >>config.log 2>&1 && answer='yes'
	eval "HAVE_$2=\"$answer\""
	printf %s\\n "$ECHOBUF ... $answer"
	rm -f -- "$TEMP_CODE" "$TEMP_EXE"
	[ "$answer" = 'no' ] && {
		[ "$4" ] && die 1 "$4"
	}
}

check_val() # $1 = language  $2 = HAVE_$2  $3 = lib  $4 = include directory [checked only if non-empty]
{	tmpval="$(eval "printf %s \"\$HAVE_$2\"")"
	oldval="$(eval "printf %s \"\$TMP_$2\"")"
	if [ "$tmpval" = 'no' ] && [ "$oldval" != 'no' ]; then
		eval "HAVE_$2=auto"
		check_lib "$1" "$2" "$3"

		if [ "${4:-}" ] && [ "$answer" = 'yes' ]; then
			val="$2"
			include="$4"
			eval "set -- $INCLUDES"
			for dir do
				[ -d "/$dir/$include" ] && { eval "${val}_CFLAGS=\"-I/$dir/$include\""; break; }
			done
			[ -z "$(eval "printf %s \"\${${val}_CFLAGS}\"")" ] && eval "HAVE_$val=no"
		fi

		if [ "$answer" = 'no' ] && [ "$oldval" = 'yes' ]; then
			die 1 "Forced to build with library $lib, but cannot locate. Exiting ..."
		fi
	fi
}

create_config_header()
{   outfile="$1"; shift

	printf %s\\n "Creating config header: $outfile"
	name="$(printf %s "QB_${outfile}__" | tr '.[a-z]' '_[A-Z]')"

	{	printf %s\\n "#ifndef $name" "#define $name" '' \
			"#define PACKAGE_NAME \"$PACKAGE_NAME\""

		while [ "$1" ]; do
			case "$(eval "printf %s \"\$HAVE_$1\"")" in
				'yes')
					if [ "$(eval "printf %s \"\$C89_$1\"")" = 'no' ]; then
						printf %s\\n '#if __cplusplus || __STDC_VERSION__ >= 199901L' \
							"#define HAVE_$1 1" '#endif'
					else
						printf %s\\n "#define HAVE_$1 1"
					fi
				;;
				'no') printf %s\\n "/* #undef HAVE_$1 */";;
			esac
			shift
		done

		eval "set -- $CONFIG_DEFINES"
		for VAR do
			printf %s\\n "#define ${VAR%%=*} ${VAR#*=}"
		done

		printf %s\\n '#endif'
	} > "$outfile"
}

create_config_make()
{	outfile="$1"; shift

	printf %s\\n "Creating make config: $outfile"

	{	[ "$USE_LANG_C" = 'yes' ] && printf %s\\n "CC = $CC" "CFLAGS = $CFLAGS"
		[ "$USE_LANG_CXX" = 'yes' ] && printf %s\\n "CXX = $CXX" "CXXFLAGS = $CXXFLAGS"

		printf %s\\n "WINDRES = $WINDRES" \
			"ASFLAGS = $ASFLAGS" \
			"LDFLAGS = $LDFLAGS" \
			"INCLUDE_DIRS = $INCLUDE_DIRS" \
			"LIBRARY_DIRS = $LIBRARY_DIRS" \
			"PACKAGE_NAME = $PACKAGE_NAME" \
			"BUILD = $BUILD" \
			"PREFIX = $PREFIX"

		while [ "$1" ]; do
			case "$(eval "printf %s \"\$HAVE_$1\"")" in
				'yes')
					if [ "$(eval "printf %s \"\$C89_$1\"")" = 'no' ]; then
						printf %s\\n "ifneq (\$(C89_BUILD),1)" \
							"HAVE_$1 = 1" 'endif'
					else
						printf %s\\n "HAVE_$1 = 1"
					fi
				;;
				'no') printf %s\\n "HAVE_$1 = 0";;
			esac
			
			case "$PKG_CONF_USED" in
				*$1*)
					FLAG="$(eval "printf %s \"\$$1_CFLAGS\"")"
					LIBS="$(eval "printf %s \"\$$1_LIBS\"")"
					[ "${FLAG}" ] && printf %s\\n "$1_CFLAGS = ${FLAG%"${FLAG##*[! ]}"}"
					[ "${LIBS}" ] && printf %s\\n "$1_LIBS = ${LIBS%"${LIBS##*[! ]}"}"
				;;
			esac
			shift
		done
		eval "set -- $MAKEFILE_DEFINES"
		for VAR do
			printf %s\\n "${VAR%%=*} = ${VAR#*=}"
		done

	} > "$outfile"
}

. qb/config.libs.sh
