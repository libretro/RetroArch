# Only add standalone functions to this file which are easy to source anywhere.

# die:
# Prints a warning or an exit error.
# $1 = exit code, use : to not exit when printing warnings
# $@ = exit or warning messages
die()
{	ret="$1"
	shift 1
	printf %s\\n "$@" >&2
	case "$ret" in
		: ) return 0 ;;
		* ) exit "$ret" ;;
	esac
}

# exists:
# Finds executable files in the $PATH
# $@ = files
exists()
{	v=1
	while [ $# -gt 0 ]; do
		arg="$1"
		shift 1
		case "$arg" in ''|*/) continue ;; esac
		x="${arg##*/}"
		z="${arg%/*}"
		[ ! -f "$z/$x" ] || [ ! -x "$z/$x" ] && [ "$z/$x" = "$arg" ] &&
			continue
		[ "$x" = "$z" ] && [ -x "$z/$x" ] && [ ! -f "$arg" ] && z=
		p=":$z:$PATH"
		while [ "$p" != "${p#*:}" ]; do
			p="${p#*:}"
			d="${p%%:*}"
			if [ -f "$d/$x" ] && [ -x "$d/$x" ]; then
				printf %s\\n "$d/$x"
				v=0
				break
			fi
		done
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
