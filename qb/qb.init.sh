# Only add standalone functions to this file which are easy to source anywhere.

# die:
# Prints a warning or an exit error.
# $1 = exit code, use : to not exit when printing warnings
# $@ = exit or warning messages
die()
{	ret="$1"
	shift 1
	case "$ret" in
		: ) printf %s\\n "$@" >&2; return 0 ;;
		0 ) printf %s\\n "$@" ;;
		* ) printf %s\\n "$@" >&2 ;;
	esac
	exit "$ret"
}

# exists:
# Finds executable files in the $PATH
# $@ = files
exists()
{	v=1
	while [ $# -gt 0 ]; do
		arg="$1"
		shift 1
		case "$arg" in
			''|*/ )
				:
			;;
			*/* )
				if [ -f "$arg" ] && [ -x "$arg" ]; then
					printf %s\\n "$arg"
					v=0
				fi
			;;
			* )
				p=":$PATH"
				while [ "$p" != "${p#*:}" ]; do
					p="${p#*:}"
					d="${p%%:*}/$arg"
					if [ -f "$d" ] && [ -x "$d" ]; then
						printf %s\\n "$d"
						v=0
						break
					fi
				done
			;;
		esac
	done
	return $v
}

# match:
# Compares a variable against a list of variables
# $1 = variable
# $@ = list of variables
match()
{
	var="$1"
	shift
	for string do
		case "$string" in
			"$var" ) return 0 ;;
		esac
	done
	return 1
}

# next:
# Check if the next argument starts with a dash
# $1 = arg
next () { case "$1" in -*) return 0 ;; *) return 1 ;; esac; }
