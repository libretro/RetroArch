CONFIG_DEFINES=''
INCLUDE_DIRS=''
LIBRARY_DIRS=''
MAKEFILE_DEFINES=''
PKG_CONF_USED=''
NL='
'
QB_DRY=''
QB_PF_N=0
QB_PF_NEXT=1
QB_PF_PIDS=''

ASFLAGS="${ASFLAGS:-}"
CFLAGS="${CFLAGS:-}"
CXXFLAGS="${CXXFLAGS:-}"
LDFLAGS="${LDFLAGS:-}"
PREFIX="${PREFIX:-/usr/local}"
SHARE_DIR="${SHARE_DIR:-${PREFIX}/share}"

# add_define:
# $1 = MAKEFILE or CONFIG
# $2 = define
# $3 = value
add_define()
{ 	eval "${1}_DEFINES=\"\${${1}_DEFINES} $2=$3\""; }

# add_dirs:
# $1 = INCLUDE or LIBRARY
# $@ = include or library paths
add_dirs()
{	ADD="$1"; LINK="${1%"${1#?}"}"; shift
	while [ $# -gt 0 ]; do
		eval "${ADD}_DIRS=\"\${${ADD}_DIRS} -${LINK}${1}\""
		shift
	done
	eval "${ADD}_DIRS=\"\${${ADD}_DIRS# }\""
	BUILD_DIRS="$INCLUDE_DIRS $LIBRARY_DIRS"
}

# check_compiler:
# $1 = language
# $2 = function in lib
check_compiler()
{	if [ "$1" = cxx ]; then
		COMPILER="$CXX"
		FLAGS="$CXXFLAGS"
		TEMP_CODE="$TEMP_CXX"
		TEST_C="extern \"C\" { void $2(void); } int main() { $2(); }"
	else
		COMPILER="$CC"
		FLAGS="$CFLAGS"
		TEMP_CODE="$TEMP_C"
		TEST_C="void $2(void); int main(void) { $2(); return 0; }"
	fi
}

# Check compiles run ahead of the checks.
#
# Before config.libs.sh runs, qb_prefetch runs it once in a subshell with
# QB_DRY set. There qb_compile records each compile the checks would run
# and reports success without running it, and nothing else that run does
# is kept. Background workers then run the recorded compiles while
# config.libs.sh runs for real, in order. A real check uses a background
# result only when its command and test program are exactly the recorded
# ones and compiles in place otherwise, so the dry run decides how much
# work runs ahead and never what configure finds. Anything config.libs.sh
# does other than through the check helpers is skipped when QB_DRY is set.
#
# QB_JOBS sets how many processors to use; 1 runs every check in place.

# qb_quote:
# Sets qb_q to $1 quoted for eval.
qb_quote()
{	qb_q=''
	qb_r="$1"
	while :; do
		case "$qb_r" in
			*\'* )
				qb_h="${qb_r%%\'*}"
				qb_q="$qb_q$qb_h'\\''"
				qb_r="${qb_r#*\'}"
			;;
			* )
				qb_q="'$qb_q$qb_r'"
				return 0
			;;
		esac
	done
}

# qb_compile:
# Runs a check's compile, adding its output to config.log.
# $1 = test program file
# $2 = test program, written to $1
# $@ = the rest: the compile command, naming $1 and $TEMP_EXE
qb_compile()
{	qb_file="$1"
	qb_text="$2"
	shift 2
	qb_key="$*"
	for qb_w do
		case "$qb_w" in
			*' '*|*'	'*|*"$NL"* ) qb_key=''; break ;;
		esac
	done

	if [ "$QB_DRY" ]; then
		[ "$qb_key" ] || return 0
		QB_PF_N=$(($QB_PF_N + 1))
		qb_quote "$qb_key"
		qb_k="$qb_q"
		qb_quote "$qb_text"
		printf %s\\n "QB_PF_FILE_$QB_PF_N=$qb_file" \
			"QB_PF_CMD_$QB_PF_N=$qb_k" \
			"QB_PF_SRC_$QB_PF_N=$qb_q" \
			"QB_PF_N=$QB_PF_N" >> "$QB_PF_SPEC"
		return 0
	fi

	if [ "$qb_key" ] && [ "$QB_PF_N" -gt 0 ]; then
		qb_c=0
		qb_i="$QB_PF_NEXT"
		while [ "$qb_c" -lt "$QB_PF_N" ]; do
			[ "$qb_i" -le "$QB_PF_N" ] || qb_i=1
			eval "qb_k=\${QB_PF_CMD_$qb_i} qb_f=\${QB_PF_FILE_$qb_i}"
			if [ "$qb_k" = "$qb_key" ] && [ "$qb_f" = "$qb_file" ]; then
				eval "qb_t=\${QB_PF_SRC_$qb_i}"
				[ "$qb_t" = "$qb_text" ] && break
			fi
			qb_i=$(($qb_i + 1))
			qb_c=$(($qb_c + 1))
		done
		if [ "$qb_c" -lt "$QB_PF_N" ]; then
			QB_PF_NEXT=$(($qb_i + 1))
			# Take it if no worker has. If one has, run compiles no one
			# has taken yet until its answer is in, as long as that
			# worker is still running.
			set -C
			{ printf %s\\n 0 > ".qb.$qb_i.claim"; } 2>/dev/null && qb_c=''
			set +C
			qb_j="$QB_PF_NEXT"
			while [ "$qb_c" ] && [ ! -s ".qb.$qb_i.rc" ]; do
				qb_wk=''
				read -r qb_wk < ".qb.$qb_i.claim"
				[ "$qb_wk" ] || continue
				[ "$qb_wk" != 0 ] || break
				eval "qb_p=\${QB_PF_PID_$qb_wk}"
				kill -0 "$qb_p" 2>/dev/null || break
				while [ "$qb_j" -le "$QB_PF_N" ] && [ -e ".qb.$qb_j.claim" ]; do
					qb_j=$(($qb_j + 1))
				done
				[ "$qb_j" -le "$QB_PF_N" ] && qb_pf_run "$qb_j" 0
			done
			if [ "$qb_c" ] && [ -s ".qb.$qb_i.rc" ]; then
				read -r qb_rc < ".qb.$qb_i.rc"
				[ -s ".qb.$qb_i.log" ] && cat ".qb.$qb_i.log" >> config.log
				return "$qb_rc"
			fi
		fi
	fi

	printf %s "$qb_text" > "$qb_file"
	"$@" >> config.log 2>&1
}

# qb_pf_run:
# Runs recorded compile $1 in a subshell unless someone has taken it.
# $1 = recorded compile
# $2 = who takes it: a worker number, or 0 for the checks themselves
qb_pf_run()
{	(	set -f -C
		{ printf %s\\n "$2" > ".qb.$1.claim"; } 2>/dev/null || exit 0
		eval "qb_key=\${QB_PF_CMD_$1} qb_file=\${QB_PF_FILE_$1} qb_text=\${QB_PF_SRC_$1}"
		qb_src=".qb.$1.${qb_file##*.}"
		qb_n="$1"
		printf %s "$qb_text" >| "$qb_src"
		set --
		for qb_w in $qb_key; do
			case "$qb_w" in
				"$qb_file" ) qb_w="$qb_src" ;;
				"$TEMP_EXE" ) qb_w=".qb.$qb_n.out" ;;
			esac
			set -- "$@" "$qb_w"
		done
		"$@" >| ".qb.$qb_n.log" 2>&1
		printf %s "$?" >| ".qb.$qb_n.rc"
	)
}

# qb_worker:
# Runs the recorded compiles no one has taken yet, in order.
# $1 = worker number
qb_worker()
{	qb_i=1
	while [ "$qb_i" -le "$QB_PF_N" ] && [ ! -e .qb.stop ]; do
		[ -e ".qb.$qb_i.claim" ] || qb_pf_run "$qb_i" "$1"
		qb_i=$(($qb_i + 1))
	done
}

# qb_prefetch:
# Records the compiles config.libs.sh will run and starts the workers.
qb_prefetch()
{	qb_jobs="${QB_JOBS:-${NUMBER_OF_PROCESSORS:-}}"
	[ "$qb_jobs" ] || qb_jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || :)"
	case "$qb_jobs" in
		''|*[!0-9]* ) qb_jobs=1 ;;
	esac
	qb_jobs=$(($qb_jobs - 1))
	[ "$qb_jobs" -gt 0 ] || return 0

	QB_PF_SPEC=.qb.spec
	rm -f -- .qb.*
	printf '' > "$QB_PF_SPEC"
	( QB_DRY=1; . qb/config.libs.sh ) </dev/null >/dev/null 2>&1
	. ./"$QB_PF_SPEC"

	[ "$qb_jobs" -le "$QB_PF_N" ] || qb_jobs="$QB_PF_N"
	while [ "$qb_jobs" -gt 0 ]; do
		qb_worker "$qb_jobs" </dev/null >/dev/null 2>&1 &
		QB_PF_PIDS="$QB_PF_PIDS $!"
		eval "QB_PF_PID_$qb_jobs=\$!"
		qb_jobs=$(($qb_jobs - 1))
	done
}

# qb_prefetch_stop:
# Stops the workers once the checks are done and waits for them.
qb_prefetch_stop()
{	[ "$QB_PF_PIDS" ] || return 0
	printf '' > .qb.stop
	for qb_p in $QB_PF_PIDS; do
		wait "$qb_p"
	done
	QB_PF_PIDS=''
}

# check_enabled:
# $1 = HAVE_$1 [Disabled 'feature' or 'feature feature1 feature2', $1 = name]
# $2 = USER_$2 [Enabled feature]
# $3 = lib
# $4 = feature
# $5 = enable lib when true, disable errors with 'user' [checked only if non-empty]
# if any HAVE_$1 is true, HAVE_$2 is enabled
# if USER_$2 is false, HAVE_$2 is disabled
# if neither of the above, it's an error
check_enabled()
{	add_opt "$2"
	eval "setval=\${HAVE_$2}"

	for val in $1; do
		eval "tmpvar=\${HAVE_$val}"
		if [ "$tmpvar" != 'no' ]; then
			if [ "$setval" != 'no' ] && match "${5:-}" true user; then
				eval "HAVE_$2=yes"
			fi
			return 0
		fi
	done

	eval "tmpval=\${USER_$2}"

	if [ "$tmpval" != 'yes' ]; then
		if [ "$setval" != 'no' ]; then
			eval "HAVE_$2=no"
			if ! match "${5:-}" true user; then
				die : "Notice: $4 disabled, $3 support will also be disabled."
			fi
		fi
		return 0
	fi

	if [ "${5:-}" != 'user' ]; then
		die 1 "Error: $4 disabled and forced to build with $3 support."
	fi
}

# check_platform:
# $1 = OS ['OS' or 'OS OS2 OS3', $1 = name]
# $2 = HAVE_$2
# $3 = feature
# $4 = enable feature when 'true', disable errors with 'user' [checked only if non-empty]
check_platform()
{	add_opt "$2"
	eval "tmpval=\${HAVE_$2}"
	[ "$tmpval" = 'no' ] && return 0

	error=
	newval=
	eval "setval=\${USER_$2}"

	for platform in $1; do
		if [ "$setval" = 'yes' ]; then
			if [ "$error" != 'no' ] && [ "${4:-}" != 'user' ] &&
					{ { [ "$platform" != "$OS" ] &&
					match "${4:-}" true user; } ||
					{ [ "$platform" = "$OS" ] &&
					! match "${4:-}" true user; }; }; then
				error='yes'
			elif match "${4:-}" true user; then
				error='no'
			fi
		elif [ "$platform" = "$OS" ]; then
			if match "${4:-}" true user; then
				newval=yes
				break
			else
				newval=no
			fi
		elif match "${4:-}" true user; then
			newval=auto
		fi
	done

	if [ "${error}" = 'yes' ]; then
		die 1 "Error: $3 not supported for $OS."
	else
		eval "HAVE_$2=\"${newval:-$tmpval}\""
	fi
}

# check_lib:
# Compiles a simple test program to check if a library is available.
# $1 = language
# $2 = HAVE_$2
# $3 = lib
# $4 = function in lib [checked only if non-empty]
# $5 = extralibs [checked only if non-empty]
# $6 = headers [checked only if non-empty]
# $7 = include directories ['dir' or 'dir dir1 dir2', each must exist,
#      checked only if non-empty]
# $8 = critical error message [checked only if non-empty]
check_lib()
{	add_opt "$2"
	eval "tmpval=\${HAVE_$2}"
	[ "$tmpval" = 'no' ] && return 0

	check_compiler "$1" "$4"

	if [ "$4" ]; then
		MSG="Checking function $4 in"
		if [ "$6" ]; then
			qb_code="$6${NL}int main(void) { void *p = (void*)$4; return 0; }$NL"
		else
			qb_code="$TEST_C$NL"
		fi
	else
		MSG='Checking existence of'
		qb_code="int main(void) { return 0; }$NL"
	fi

	lib="${3% }"
	include="${7:-}"
	error="${8:-}"
	answer='no'

	printf %s "$MSG $lib ... "

	qb_compile "$TEMP_CODE" "$qb_code" $COMPILER -o "$TEMP_EXE" "$TEMP_CODE" \
		$BUILD_DIRS $5 $FLAGS $LDFLAGS $lib && answer='yes'

	printf %s\\n "$answer"

	if [ "$answer" = 'yes' ] && [ "$include" ]; then
		incflags=''
		for inc in $include; do
			answer='no'
			for dir in $INCLUDES; do
				[ "$answer" = 'yes' ] && break
				printf %s "Checking existence of /$dir/$inc ... "
				if [ -d "/$dir/$inc" ]; then
					incflags="${incflags:+$incflags }-I/$dir/$inc"
					answer='yes'
				fi
				printf %s\\n "$answer"
			done
			[ "$answer" = 'yes' ] || break
		done
		[ "$answer" = 'yes' ] && eval "${2}_CFLAGS=\"$incflags\""
	fi

	eval "HAVE_$2=\"$answer\""

	if [ "$answer" = 'no' ]; then
		[ "$error" ] && die 1 "$error"
		eval "setval=\${USER_$2}"
		if [ "$setval" = 'yes' ]; then
			die 1 "Forced to build with library $lib, but cannot locate. Exiting ..."
		fi
	else
		eval "${2}_LIBS=\"$lib\""
		PKG_CONF_USED="$PKG_CONF_USED $2"
	fi

	return 0
}

# check_pkgconf:
# If available uses $PKG_CONF_PATH to find a library.
# $1 = HAVE_$1
# $2 = package ['package' or 'package package1 package2', $1 = name]
# $3 = version [checked only if non-empty]
# $4 = critical error message [checked only if non-empty]
# $5 = force check_lib when true [checked only if non-empty, set by check_val]
check_pkgconf()
{	add_opt "$1"
	eval "tmpval=\${HAVE_$1}"
	eval "TMP_$1=\$tmpval"
	[ "$tmpval" = 'no' ] && return 0

	ECHOBUF=''
	[ "${3:-}" ] && ECHOBUF=" >= ${3##* }"

	pkg="${2%% *}"
	MSG='Checking presence of package'

	[ "$PKG_CONF_PATH" = "none" ] && {
		eval "HAVE_$1=no"
		eval "${1#HAVE_}_VERSION=0.0"
		printf %s\\n "$MSG $pkg$ECHOBUF ... no"
		# A package asked for by name is an error without pkg-config
		# too, unless something that can still find it follows: a
		# library check, or a check_nopkg probe.
		eval "setval=\${USER_$1}"
		if [ "${5:-}" != 'true' ] && [ "${6:-}" != 'nopkg' ] && \
		   [ "$setval" = 'yes' ]; then
			die 1 "Forced to build with package $pkg, but pkg-config was not found. Exiting ..."
		fi
		return 0
	}

	if [ "$QB_DRY" ]; then
		eval "HAVE_$1=yes"
		return 0
	fi

	ver="${3:-0.0}"
	err="${4:-}"
	lib="${5:-}"
	answer='no'
	version='no'

	for pkgnam in ${2#* }; do
		[ "$answer" = 'yes' ] && break
		printf %s "$MSG $pkgnam$ECHOBUF ... "
		for pkgver in $ver; do
			if "$PKG_CONF_PATH" --atleast-version="$pkgver" "$pkgnam"; then
				answer='yes'
				version="$("$PKG_CONF_PATH" --modversion "$pkgnam")"
				eval "${1}_CFLAGS=\"$("$PKG_CONF_PATH" --cflags "$pkgnam")\""
				eval "${1}_LIBS=\"$("$PKG_CONF_PATH" --libs "$pkgnam")\""
				eval "${1}_VERSION=\"$pkgver\""
				break
			fi
		done
		printf %s\\n "$version"
	done

	eval "HAVE_$1=\"$answer\""

	if [ "$answer" = 'no' ]; then
		[ "$lib" != 'true' ] || return 0
		[ "$err" ] && die 1 "$err"
		eval "setval=\${USER_$1}"
		if [ "$setval" = 'yes' ]; then
			die 1 "Forced to build with package $pkg, but cannot locate. Exiting ..."
		fi
	else
		PKG_CONF_USED="$PKG_CONF_USED $1"
	fi
}

# check_header:
# $1 = language
# $2 = HAVE_$2
# $@ = header files
check_header()
{	add_opt "$2"
	check_compiler "$1" ''
	eval "tmpval=\${HAVE_$2}"
	[ "$tmpval" = 'no' ] && return 0
	val="$2"
	header="$3"
	shift 2
	qb_code=''
	for head do
		CHECKHEADER="$head"
		qb_code="$qb_code#include <$head>$NL"
	done
	qb_code="${qb_code}int main(void) { return 0; }$NL"
	answer='no'
	printf %s "Checking presence of header file $CHECKHEADER ... "
	qb_compile "$TEMP_CODE" "$qb_code" $COMPILER -c -o "$TEMP_EXE" "$TEMP_CODE" \
		$BUILD_DIRS $FLAGS $LDFLAGS && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$answer"
	eval "setval=\${USER_$val}"
	if [ "$setval" = 'yes' ] && [ "$answer" = 'no' ]; then
		die 1 "Build assumed that $header exists, but cannot locate. Exiting ..."
	fi
}

# check_macro:
# $1 = HAVE_$1
# $2 = macro name
# $3 = header name [included only if non-empty]
check_macro()
{	add_opt "$1"
	eval "tmpval=\${HAVE_$1}"
	[ "$tmpval" = 'no' ] && return 0
	header_include=''
	ECHOBUF=''
	if [ "${3:-}" ]; then
		header_include="#include <$3>"
		ECHOBUF=" in $3"
	fi
	qb_code="$header_include$NL#ifndef $2$NL#error $2 is not defined$NL#endif${NL}int main(void) { return 0; }$NL"
	answer='no'
	val="$1"
	macro="$2"
	printf %s "Checking presence of predefined macro $macro$ECHOBUF ... "
	qb_compile "$TEMP_C" "$qb_code" $CC -c -o "$TEMP_EXE" "$TEMP_C" \
		$BUILD_DIRS $CFLAGS $LDFLAGS && answer='yes'
	eval "HAVE_$val=\"$answer\""
	printf %s\\n "$answer"
	eval "setval=\${USER_$val}"
	if [ "$setval" = 'yes' ] && [ "$answer" = 'no' ]; then
		die 1 "Build assumed that $macro is defined, but it's not. Exiting ..."
	fi
}

# check_switch:
# $1 = language
# $2 = HAVE_$2
# $3 = switch
# $4 = critical error message [checked only if non-empty]
check_switch()
{	add_opt "$2"
	check_compiler "$1" ''

	answer='no'
	printf %s "Checking for availability of switch $3 in $COMPILER ... "
	qb_compile "$TEMP_CODE" "int main(void) { return 0; }$NL" \
		$COMPILER -o "$TEMP_EXE" "$TEMP_CODE" \
		$BUILD_DIRS $CFLAGS $3 -Werror $LDFLAGS && answer='yes'
	eval "HAVE_$2=\"$answer\""
	printf %s\\n "$answer"
	if [ "$answer" = 'yes' ]; then
		eval "${2}_CFLAGS=\"$3\""
		PKG_CONF_USED="$PKG_CONF_USED $2"
	elif [ "${4:-}" ]; then
		die 1 "$4"
	fi
}

# check_val:
# Uses check_pkgconf to find a library and falls back to check_lib if false.
# $1 = language
# $2 = HAVE_$2
# $3 = lib
# $4 = include directories [see check_lib, checked only if non-empty]
# $5 = package
# $6 = version [checked only if non-empty]
# $7 = critical error message [checked only if non-empty]
# $8 = force check_lib when true [checked only if non-empty]
# check_nopkg: finds a package that has no pkg-config to describe it.
# Only acts when pkg-config is absent, the package was not disabled, and
# nothing found it already. The include directories are looked for
# under every include directory configure knows and under the library
# directories too, where some packages keep a generated header; an
# absolute one is taken as it is. The probe program must build and
# link against them and the libraries for the package to count, and it
# then gets the CFLAGS and LIBS pkg-config would have given it.
#
# $1 = language ('' for C, 'cxx' for C++)
# $2 = NAME, as for check_pkgconf
# $3 = libraries to link, e.g. '-ldbus-1'
# $4 = include directories, e.g. 'dbus-1.0 dbus-1.0/include'; an entry
#      starting with '-' is a flag and is passed through as it is
# $5 = probe program
# $6 = extra compiler flags for the probe only (optional)
check_nopkg()
{	[ "$PKG_CONF_PATH" = 'none' ] || return 0
	eval "tmpval=\${TMP_$2}"
	[ "$tmpval" = 'no' ] && return 0
	eval "tmpval=\${HAVE_$2}"
	[ "$tmpval" = 'yes' ] && return 0

	check_compiler "$1" ''
	nopkg_flags=''
	nopkg_triplet="$("$CC" -print-multiarch 2>/dev/null || :)"
	nopkg_dirs="$INCLUDES${nopkg_triplet:+ usr/lib/$nopkg_triplet} usr/lib64 usr/lib usr/local/lib64 usr/local/lib"
	answer='yes'

	for nopkg_inc in $4; do
		case "$nopkg_inc" in
			-* )
				nopkg_flags="${nopkg_flags:+$nopkg_flags }$nopkg_inc"
				continue
			;;
			/* )
				if [ -d "$nopkg_inc" ]; then
					nopkg_flags="${nopkg_flags:+$nopkg_flags }-I$nopkg_inc"
					continue
				fi
			;;
			* )
				nopkg_found=''
				for nopkg_dir in $nopkg_dirs; do
					if [ -d "/$nopkg_dir/$nopkg_inc" ]; then
						nopkg_found="-I/$nopkg_dir/$nopkg_inc"
						break
					fi
				done
				if [ "$nopkg_found" ]; then
					nopkg_flags="${nopkg_flags:+$nopkg_flags }$nopkg_found"
					continue
				fi
			;;
		esac
		answer='no'
		break
	done

	printf %s "Checking for $2 without pkg-config ... "
	if [ "$answer" = 'yes' ]; then
		answer='no'
		qb_compile "$TEMP_CODE" "$5$NL" $COMPILER -o "$TEMP_EXE" "$TEMP_CODE" \
			$BUILD_DIRS ${6:-} $nopkg_flags $FLAGS $LDFLAGS $3 && answer='yes'
	fi
	printf %s\\n "$answer"

	if [ "$answer" = 'yes' ]; then
		eval "HAVE_$2=yes"
		eval "${2}_CFLAGS=\"$nopkg_flags\""
		eval "${2}_LIBS=\"$3\""
		PKG_CONF_USED="$PKG_CONF_USED $2"
		return 0
	fi
	eval "setval=\${USER_$2}"
	if [ "$setval" = 'yes' ]; then
		die 1 "Forced to build with $2, but it cannot be found without pkg-config. Exiting ..."
	fi
	return 0
}

# nopkg_version_ge: true when dotted version $1 is at least $2.
nopkg_version_ge()
{	printf '%s\n%s\n' "$1" "$2" | awk -F. '
		NR == 1 { for (i = 1; i <= 4; i++) a[i] = $i + 0 }
		NR == 2 { for (i = 1; i <= 4; i++) b[i] = $i + 0 }
		END {
			for (i = 1; i <= 4; i++) {
				if (a[i] > b[i]) exit 0
				if (a[i] < b[i]) exit 1
			}
			exit 0
		}'
}

# nopkg_qmake: prints the qmake of Qt major version $1, if there is one;
# it answers for its own headers and libraries without pkg-config.
nopkg_qmake()
{	for nopkg_q in "qmake$1" "qmake-qt$1" qmake; do
		nopkg_q="$(exists "${CROSS_COMPILE:-}$nopkg_q" || :)"
		[ "$nopkg_q" ] || continue
		case "$("$nopkg_q" -query QT_VERSION 2>/dev/null)" in
			"$1".* ) printf %s\\n "$nopkg_q"; return 0 ;;
		esac
	done
	return 1
}

# moc_probe:
# Finds a moc that works with the Qt found and the C++ compiler. Writes
# its log to stdout and the moc it settled on to $TEMP_MOC_RES, and
# returns 0 when that moc works.
moc_probe()
{	moc_works=1
	if [ "$MOC" ]; then
		QT_SELECT="$QT_VERSION" \
		"$MOC" -o "$TEMP_CPP" "$TEMP_MOC" 2>&1 &&
			$CXX -o "$TEMP_MOC_OBJ" \
			$QT_FLAGS -fPIC -c "$TEMP_CPP" 2>&1 &&
		moc_works=0
	else
		if [ "$QT_VERSION" = "qt6" ]; then
			QMAKE="$(exists qmake6)" || QMAKE="qmake"
			$QMAKE -query QT_HOST_LIBEXECS 2>&1 && QT_HOST_LIBEXECS="$($QMAKE -query QT_HOST_LIBEXECS)/"
		fi
		for moc in "${QT_HOST_LIBEXECS}moc-$QT_VERSION" "${QT_HOST_LIBEXECS}moc"; do
			MOC="$(exists "$moc")" || MOC=""
			if [ "$MOC" ]; then
				QT_SELECT="$QT_VERSION" \
				"$MOC" -o "$TEMP_CPP" "$TEMP_MOC" 2>&1 ||
					continue
				if $CXX -o "$TEMP_MOC_OBJ" \
						$QT_FLAGS -fPIC -c \
						"$TEMP_CPP" 2>&1; then
					moc_works=0
					break
				fi
			fi
		done
	fi
	printf %s\\n "$MOC" > "$TEMP_MOC_RES"
	return $moc_works
}

# moc_start:
# Starts the moc check in the background once Qt is settled, so it runs
# alongside the checks that follow. qb.moc.sh collects the answer and
# adds its log to config.log where the check is reported.
moc_start()
{	[ "$HAVE_QT" = 'yes' ] && [ -z "$QB_DRY" ] || return 0
	. qb/config.moc.sh
	MOC_SIG="$CXX|$QT_VERSION|$QT_FLAGS"
	printf %s\\n '#include <QTimeZone>' \
		'class Test : public QObject' \
		'{' \
		'public:' \
		'   Q_OBJECT' \
		'   QTimeZone tz;' \
		'};' > "$TEMP_MOC"
	moc_probe > "$TEMP_MOC_LOG" 2>&1 < /dev/null &
	MOC_PID=$!
}

check_val()
{	# Without pkg-config the library check below always runs, and it is
	# what decides a package forced by name.
	if [ "$PKG_CONF_PATH" = "none" ]; then
		check_pkgconf "$2" "$5" "${6:-}" "${7:-}" true
	else
		check_pkgconf "$2" "$5" "${6:-}" "${7:-}" "${8:-}"
	fi
	[ "$PKG_CONF_PATH" = "none" ] || [ "${8:-}" = true ] || return 0
	eval "tmpval=\${HAVE_$2}"
	eval "oldval=\${TMP_$2}"
	if [ "$tmpval" = 'no' ] && [ "$oldval" != 'no' ]; then
		eval "HAVE_$2=auto"
		check_lib "$1" "$2" "$3" '' '' '' "${4:-}" "${7:-}"
	fi
}

create_config_header()
{   outfile="$1"; shift

	printf %s\\n "Creating config header: $outfile"
	name="$(printf %s "QB_${outfile}__" | tr '.[a-z]' '_[A-Z]')"

	{	printf %s\\n "#ifndef $name" "#define $name" '' \
			"#define PACKAGE_NAME \"$PACKAGE_NAME\""

		while [ $# -gt 0 ]; do
			eval "have=\${HAVE_$1}"
			case "$have" in
				'yes')
					n='0'
					eval "c89_build=\${C89_$1}"
					eval "cxx_build=\${CXX_$1}"

					if [ "$c89_build" = 'no' ]; then
						n=$(($n+1))
						printf %s\\n '#if __cplusplus || __STDC_VERSION__ >= 199901L'
					fi

					if [ "$cxx_build" = 'no' ]; then
						n=$(($n+1))
						printf %s\\n '#ifndef CXX_BUILD'
					fi

					printf %s\\n "#define HAVE_$1 1"

					while [ $n != '0' ]; do
						n=$(($n-1))
						printf %s\\n '#endif'
					done
				;;
				'no') printf %s\\n "/* #undef HAVE_$1 */";;
			esac
			shift
		done

		for VAR in $CONFIG_DEFINES; do
			printf %s\\n "#define ${VAR%%=*} ${VAR#*=}"
		done

		printf %s\\n '#endif'
	} > "$outfile"
}

create_config_make()
{	outfile="$1"; shift

	printf %s\\n "Creating make config: $outfile"

	{	if [ "$HAVE_CC" = 'yes' ]; then
			printf %s\\n "CC = $CC"

			if [ "${CFLAGS}" ]; then
				printf %s\\n "CFLAGS = $CFLAGS"
			fi
		fi

		if [ "$HAVE_CXX" = 'yes' ]; then
			printf %s\\n "CXX = $CXX"

			if [ "${CXXFLAGS}" ]; then
				printf %s\\n "CXXFLAGS = $CXXFLAGS"
			fi
		fi

		printf %s\\n "WINDRES = $WINDRES" \
			"MOC = $MOC" \
			"ASFLAGS = $ASFLAGS" \
			"LDFLAGS = $LDFLAGS" \
			"INCLUDE_DIRS = $INCLUDE_DIRS" \
			"LIBRARY_DIRS = $LIBRARY_DIRS" \
			"PACKAGE_NAME = $PACKAGE_NAME" \
			"BUILD = $BUILD" \
			"PREFIX = $PREFIX"

		while [ $# -gt 0 ]; do
			eval "have=\${HAVE_$1}"
			case "$have" in
				'yes')
					n='0'
					c89_build="C89_$1"
					cxx_build="CXX_$1"

					for build in "$c89_build" "$cxx_build"; do
						eval "bval=\${$build}"
						if [ "$bval" = 'no' ]; then
							n=$(($n+1))
							printf %s\\n "ifneq (\$(${build%%_*}_BUILD),1)"
						fi
					done

					printf %s\\n "HAVE_$1 = 1"

					while [ $n != '0' ]; do
						n=$(($n-1))
						printf %s\\n 'endif'
					done
				;;
				'no') printf %s\\n "HAVE_$1 = 0";;
			esac

			case "$PKG_CONF_USED" in
				*$1*)
					eval "FLAG=\${$1_CFLAGS}"
					eval "LIBS=\${$1_LIBS}"
					[ "${FLAG}" ] && printf %s\\n "$1_CFLAGS = ${FLAG%"${FLAG##*[! ]}"}"
					[ "${LIBS}" ] && printf %s\\n "$1_LIBS = ${LIBS%"${LIBS##*[! ]}"}"
				;;
			esac
			shift
		done

		for VAR in $MAKEFILE_DEFINES; do
			printf %s\\n "${VAR%%=*} = ${VAR#*=}"
		done

	} > "$outfile"
}

qb_prefetch
. qb/config.libs.sh
qb_prefetch_stop
