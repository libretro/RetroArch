# Creates config.mk and config.h.
vars=''
add_define MAKEFILE GLOBAL_CONFIG_DIR "$GLOBAL_CONFIG_DIR"
eval "set -- $CONFIG_OPTS"
while [ $# -gt 0 ]; do
	tmpvar="${1%=*}"
	shift 1
	var="${tmpvar#HAVE_}"
	vars="${vars} $var"
done
VARS="$(printf %s "$vars" | tr ' ' '\n' | $SORT)"
create_config_make config.mk $(printf %s "$VARS")
create_config_header config.h $(printf %s "$VARS")
