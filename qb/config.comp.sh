USE_LANG_C="yes"

# C++ compiler is optional in other platforms supported by ./configure
if [ "$OS" = 'Win32' ]; then
	USE_LANG_CXX="yes"
fi

