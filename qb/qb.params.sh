# check_build:
# $1 = USER_$1
check_build()
{	setval="$(eval "printf %s \"\$USER_$1\"")"

	c89_build="C89_$1"
	cxx_build="CXX_$1"

	for build in "$c89_build" "$cxx_build"; do
		C="${build%%_*}"
		tmpval="$(eval "printf %s \"\$$build\"")"
		tmpvar="$(eval "printf %s \"\$HAVE_${C}_BUILD\"")"

		if [ "$tmpval" = 'no' ] && [ "$tmpvar" = 'yes' ]; then
			if [ "$setval" = 'yes' ]; then
				msg="$(eval "printf %s \"\$MSG_${C}_$1\"")"
				die 1 "Error: $msg not supported with $C builds."
			else
				eval "HAVE_$1=no"
				eval "HAVE_${C}_$1=no"
			fi
		fi
	done
}

# add_opt
# $1 = HAVE_$1
# $2 = value ['auto', 'no' or 'yes', checked only if non-empty]
add_opt()
{	check_build "$1"
	setval="$(eval "printf %s \"\$USER_$1\"")"
	[ "${2:-}" ] && ! match "$setval" no yes && eval "HAVE_$1=\"$2\""

	for opt in $(printf %s "$CONFIG_OPTS"); do
		case "$opt" in
			"$1") return 0 ;;
		esac
	done

	CONFIG_OPTS="${CONFIG_OPTS} $1"
}

# print_help_option
# $1 = option
# $@ = description
print_help_option()
{
	_opt="$1"
	shift 1
	printf '  %-26s  %s\n' "$_opt" "$@"
}

print_help()
{	cat << EOF
====================
 Quickbuild script
====================
Package: $PACKAGE_NAME

General environment variables:
  CC:         C compiler
  CFLAGS:     C compiler flags
  CXX:        C++ compiler
  CXXFLAGS:   C++ compiler flags
  LDFLAGS:    Linker flags

General options:
EOF
	print_help_option "--prefix=PATH"            "Install path prefix"
	print_help_option "--sysconfdir=PATH"        "System wide config file prefix"
	print_help_option "--bindir=PATH"            "Binary install directory"
	print_help_option "--datarootdir=PATH"       "Read-only data install directory"
	print_help_option "--docdir=PATH"            "Documentation install directory"
	print_help_option "--mandir=PATH"            "Manpage install directory"
	print_help_option "--global-config-dir=PATH" "System wide config file prefix (Deprecated)"
	print_help_option "--build=BUILD"            "The build system (no-op)"
	print_help_option "--host=HOST"              "Cross-compile with HOST-gcc instead of gcc"
	print_help_option "--help"                   "Show this help"

	printf %s\\n '' 'Custom options:'

	while read -r VAR _ COMMENT; do
		case "$VAR" in
			'C89_'*|'CXX_'*) continue;;
			*)
			TMPVAR="${VAR%=*}"
			VAL="${VAR#*=}"
			VAR="$(printf %s "${TMPVAR#HAVE_}" | tr '[:upper:]' '[:lower:]')"
			case "$VAL" in
				'yes'*)
					print_help_option "--disable-$VAR" "Disable  $COMMENT";;
				'no'*)
					print_help_option "--enable-$VAR" "Enable   $COMMENT";;
				'auto'*)
					print_help_option "--enable-$VAR" "Enable   $COMMENT"
					print_help_option "--disable-$VAR" "Disable  $COMMENT";;
				*)
					print_help_option "--with-$VAR" "Config   $COMMENT";;
			esac
		esac
	done < 'qb/config.params.sh'
}

# $opt is returned if exists in OPTS
opt_exists()
{	opt="$(printf %s "$1" | tr '[:lower:]' '[:upper:]')"
	err="$2"
	eval "set -- $OPTS"
	for OPT do [ "$opt" = "$OPT" ] && return; done
	die 1 "Unknown option $err"
}

# Parse config.params.sh
parse_opts()
{	BUILD=''
	OPTS=''
	CONFIG_OPTS=''
	config_opts='./configure'

	while read -r VAR _ COMMENT; do
		TMPVAR="${VAR%=*}"
		NEWVAR="${TMPVAR##HAVE_}"
		OPTS="${OPTS} $NEWVAR"
		case "$TMPVAR" in
			HAVE_*) CONFIG_OPTS="${CONFIG_OPTS} $NEWVAR" ;;
			C89_*|CXX_*) eval "MSG_${TMPVAR%%_*}_${TMPVAR#*_}=\"$COMMENT\"";;
		esac
		eval "USER_$NEWVAR=auto"
		eval "C89_$NEWVAR=auto"
		eval "CXX_$NEWVAR=auto"
	done < 'qb/config.params.sh'
	#OPTS contains all available options in config.params.sh - used to speedup
	#things in opt_exists()
}

# Parse user input
parse_input()
{	while [ $# -gt 0 ]; do
		config_opts="${config_opts} $1"
		case "$1" in
			--prefix=*) PREFIX=${1##--prefix=};;
			--global-config-dir=*|--sysconfdir=*) GLOBAL_CONFIG_DIR="${1#*=}";;
			--bindir=*) BIN_DIR="${1#*=}";;
			--build=*) BUILD="${1#*=}";;
			--datarootdir=*) SHARE_DIR="${1#*=}";;
			--docdir=*) DOC_DIR="${1#*=}";;
			--host=*) CROSS_COMPILE=${1##--host=}-;;
			--mandir=*) MAN_DIR="${1#*=}";;
			--enable-*)
				opt_exists "${1##--enable-}" "$1"
				eval "HAVE_$opt=yes"
				eval "USER_$opt=yes"
				case "$opt" in
					C89_BUILD) HAVE_CXX_BUILD=no;;
					CXX_BUILD) HAVE_C89_BUILD=no;;
				esac
			;;
			--disable-*)
				opt_exists "${1##--disable-}" "$1"
				eval "HAVE_$opt=no"
				eval "USER_$opt=no"
				add_opt "NO_$opt" yes
			;;
			--with-*)
				arg="${1##--with-}"
				val="${arg##*=}"
				opt_exists "${arg%%=*}" "$1"
				eval "$opt=\"$val\""
			;;
			-h|--help) print_help; exit 0;;
			--) break ;;
			'') : ;;
			*) die 1 "Unknown option $1";;
		esac
		shift
	done

	cat > config.log << EOF
Command line invocation:

  \$ ${config_opts}

## ----------- ##
## Core Tests. ##
## ----------- ##

EOF
}

parse_opts

. qb/config.params.sh

parse_input "$@"
