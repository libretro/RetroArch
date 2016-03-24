print_help_option() # $1 = option $@ = description
{
	_opt="$1"
	shift 1
	printf "  %-24s  %s\n" "$_opt" "$@"
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
	print_help_option "--global-config-dir=PATH" "System wide config file prefix"
	print_help_option "--host=HOST"              "cross-compile to build programs to run on HOST"
	print_help_option "--help"                   "Show this help"

	echo ""
	echo "Custom options:"

	while IFS='=#' read VAR VAL COMMENT; do
		VAR=$(echo "${VAR##HAVE_}" | tr '[A-Z]' '[a-z]')
		case "$VAR" in
			'c89_'*) true;;
			*)
			case "$VAL" in
				'yes'*)
					print_help_option "--disable-$VAR" "Disable $COMMENT";;
				'no'*)
					print_help_option "--enable-$VAR" "Enable $COMMENT";;
				'auto'*)
					print_help_option "--enable-$VAR" "Enable $COMMENT"
					print_help_option "--disable-$VAR" "Disable $COMMENT";;
				*)
					print_help_option "--with-$VAR" "$COMMENT";;
			esac
		esac
	done < 'qb/config.params.sh'
}

opt_exists() # $opt is returned if exists in OPTS
{	opt=$(echo "$1" | tr '[a-z]' '[A-Z]')
	for OPT in $OPTS; do [ "$opt" = "$OPT" ] && return; done
	echo "Unknown option $2"; exit 1
}

parse_input() # Parse stuff :V
{	OPTS=; while IFS='=' read VAR dummy; do OPTS="$OPTS ${VAR##HAVE_}"; done < 'qb/config.params.sh'
#OPTS contains all available options in config.params.sh - used to speedup
#things in opt_exists()
	
	while [ "$1" ]; do
		case "$1" in
			--prefix=*) PREFIX=${1##--prefix=};;
			--global-config-dir=*) GLOBAL_CONFIG_DIR=${1##--global-config-dir=};;
			--host=*) CROSS_COMPILE=${1##--host=}-;;
			--enable-*)
				opt_exists "${1##--enable-}" "$1"
				eval "HAVE_$opt=yes"
			;;
			--disable-*)
				opt_exists "${1##--disable-}" "$1"
				eval "HAVE_$opt=no"
			;;
			--with-*)
				arg="${1##--with-}"
				val="${arg##*=}"
				opt_exists "${arg%%=*}" "$1"
				eval "$opt=\"$val\""
			;;
			-h|--help) print_help; exit 0;;
			*) echo "Unknown option $1"; exit 1;;
		esac
		shift
	done
}

. qb/config.params.sh

parse_input "$@" 
