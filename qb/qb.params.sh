# add_opt
# $1 = HAVE_$1
# $2 = value ['auto', 'no' or 'yes', checked only if non-empty]
add_opt()
{	setval="$(eval "printf %s \"\$USER_$1\"")"
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

opt_exists() # $opt is returned if exists in OPTS
{	opt="$(printf %s "$1" | tr '[:lower:]' '[:upper:]')"
	err="$2"
	eval "set -- $OPTS"
	for OPT do [ "$opt" = "$OPT" ] && return; done
	die 1 "Unknown option $err"
}

parse_input() # Parse stuff :V
{	BUILD=''
	OPTS=''
	CONFIG_OPTS=''
	config_opts='./configure'

	while read -r VAR _; do
		TMPVAR="${VAR%=*}"
		NEWVAR="${TMPVAR##HAVE_}"
		OPTS="${OPTS} $NEWVAR"
		case "$TMPVAR" in
			HAVE_*) CONFIG_OPTS="${CONFIG_OPTS} $NEWVAR" ;;
		esac
		eval "USER_$NEWVAR=auto"
	done < 'qb/config.params.sh'
	#OPTS contains all available options in config.params.sh - used to speedup
	#things in opt_exists()

	while [ $# -gt 0 ]; do
		config_opts="${config_opts} $1"
		case "$1" in
			--prefix=*) PREFIX=${1##--prefix=};;
			--sysconfdir=*) GLOBAL_CONFIG_DIR="${1#*=}";;
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

. qb/config.params.sh

parse_input "$@"
