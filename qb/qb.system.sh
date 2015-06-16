
if [ -n "$CROSS_COMPILE" ]; then
	case "$CROSS_COMPILE" in
		*'-mingw32'*) OS='Win32';;
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
		*) OS="Win32";;
	esac
fi

DISTRO=''
if [ -e /etc/lsb-release ]; then
	. /etc/lsb-release
	DISTRO="(${DISTRIB_DESCRIPTION} ${DISTRIB_RELEASE})"
fi

echo "Checking operating system ... $OS ${DISTRO}"

