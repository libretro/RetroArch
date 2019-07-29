# Creates config.mk and config.h.
vars=''
add_define MAKEFILE GLOBAL_CONFIG_DIR "$GLOBAL_CONFIG_DIR"

eval "set -- $CONFIG_OPTS"
while [ $# -gt 0 ]; do
	tmpvar="${1%=*}"
	shift 1
	var="${tmpvar#HAVE_}"

	# Catch any variables not handled in add_opt.
	check_build "$var"

	# Make sure HAVE_$var has not been set to 'yes'
	# if it has been disabled by C89_BUILD or CXX_BUILD
	c89="$(eval "printf %s \"\$HAVE_C89_$var\"")"
	cxz="$(eval "printf %s \"\$HAVE_CXX_$var\"")"
	if [ "$c89" = 'no' ] || [ "$cxx" = 'no' ]; then
		eval "HAVE_$var=no"
	fi

	vars="${vars} $var"
done

VARS="$(printf %s "$vars" | tr ' ' '\n' | $SORT)"
create_config_make config.mk $(printf %s "$VARS")
create_config_header config.h $(printf %s "$VARS")
