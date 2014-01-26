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
--prefix=\$path: Install path prefix
--global-config-dir=\$path: System wide config file prefix
--global-fbdev=\$path; Framebuffer device to use, default is (/dev/fb0)
--host=HOST: cross-compile to build programs to run on HOST
--help: Show this help

Custom options:
EOF
	while IFS='=#' read VAR VAL COMMENT; do
		VAR=$(echo "${VAR##HAVE_}" | tr '[A-Z]' '[a-z]')
		case "$VAL" in
			'yes'*)
				echo "--disable-$VAR: $COMMENT";;
			'no'*)
				echo "--enable-$VAR: $COMMENT";;
			'auto'*)
				echo "--enable-$VAR: $COMMENT"
				echo "--disable-$VAR";;
			*)
				echo "--with-$VAR: $COMMENT";;
		esac
	done < 'qb/config.params.sh'
}

opt_exists() # $opt is returned if exists in OPTS
{	opt=$(echo "$1" | tr '[a-z]' '[A-Z]')
	for OPT in $OPTS; do [ "$opt" = "$OPT" ] && return; done
	print_help; exit 1
}

parse_input() # Parse stuff :V
{	OPTS=; while IFS='=' read VAR dummy; do OPTS="$OPTS ${VAR##HAVE_}"; done < 'qb/config.params.sh'
#OPTS contains all available options in config.params.sh - used to speedup
#things in opt_exists()
	
	while [ "$1" ]; do
		case "$1" in
			--prefix=*) PREFIX=${1##--prefix=};;
			--global-config-dir=*) GLOBAL_CONFIG_DIR=${1##--global-config-dir=};;
                        --global-fbdev=*) GLOBAL_FBDEV=${1##--global-fbdev=};;
			--host=*) CROSS_COMPILE=${1##--host=}-;;
			--enable-*)
				opt_exists "${1##--enable-}"
				eval "HAVE_$opt=yes"
			;;
			--disable-*)
				opt_exists "${1##--disable-}"
				eval "HAVE_$opt=no"
			;;
			--with-*)
				arg=${1##--with-}
				val=${arg##*=}
				opt_exists "${arg%%=*}"
				eval "$opt=$val"
			;;
			-h|--help) print_help; exit 0;;
			*) print_help; exit 1;;
		esac
		shift
	done
}

. qb/config.params.sh

parse_input "$@" 
