exists() # checks executables listed in $@ against the $PATH
{
	v=1
	while [ "$#" -gt 0 ]; do
		arg="$1"
		shift 1
		case "$arg" in ''|*/) continue ;; esac
		x="${arg##*/}"
		z="${arg%/*}"
		[ ! -f "$z/$x" ] || [ ! -x "$z/$x" ] && [ "$z/$x" = "$arg" ] && continue
		[ "$x" = "$z" ] && [ -x "$z/$x" ] && [ ! -f "$arg" ] && z=
		p=":$z:$PATH"
		while [ "$p" != "${p#*:}" ]; do
			p="${p#*:}"
			d="${p%%:*}"
			{ [ -f "$d/$x" ] && [ -x "$d/$x" ] && \
				{ printf %s\\n "$d/$x"; v=0; break; }; } || :
		done
	done
	return "$v"
}

if [ -n "$CROSS_COMPILE" ]; then
	case "$CROSS_COMPILE" in
		*'-mingw32'*) OS='Win32';;
		*'-msdosdjgpp'*) OS='DOS';;
		*);;
	esac
fi

if [ -z "$CROSS_COMPILE" ] || [ -z "$OS" ]; then
	case "$(uname)" in
		'Linux') OS='Linux';;
		*'BSD') OS='BSD';;
		'Darwin') OS='Darwin';;
		'CYGWIN'*) OS='Cygwin';;
		'Haiku') OS='Haiku';;
		'MINGW'*) OS='Win32';;
		'SunOS') OS='SunOS';;
		*) OS="Win32";;
	esac
fi

DISTRO=''
if [ -e /etc/lsb-release ]; then
	. /etc/lsb-release
	DISTRO="(${DISTRIB_DESCRIPTION} ${DISTRIB_RELEASE})"
fi

echo "Checking operating system ... $OS ${DISTRO}"
