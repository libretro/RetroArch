. qb/qb.params.sh

PACKAGE_NAME=ssnes
PACKAGE_VERSION=0.1

# Adds a command line opt to ./configure --help
# $1: Variable (HAVE_ALSA, HAVE_OSS, etc)   
# $2: Comment                 
# $3: Default arg. auto implies that HAVE_ALSA will be set according to library checks later on.
add_command_line_string LIBSNES "libsnes library used" "-lsnes"
add_command_line_enable FILTERS "Disable CPU filter support" yes
add_command_line_enable CG "Enable CG shader support" auto
add_command_line_enable ALSA "Enable ALSA support" auto
add_command_line_enable OSS "Enable OSS support" auto
add_command_line_enable RSOUND "Enable RSound support" auto
add_command_line_enable ROAR "Enable RoarAudio support" auto
add_command_line_enable AL "Enable OpenAL support" auto
