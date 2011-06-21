. qb/qb.params.sh

PACKAGE_NAME=ssnes
PACKAGE_VERSION=0.6

# Adds a command line opt to ./configure --help
# $1: Variable (HAVE_ALSA, HAVE_OSS, etc)   
# $2: Comment                 
# $3: Default arg. auto implies that HAVE_ALSA will be set according to library checks later on.
add_command_line_enable DYNAMIC "Enable dynamic loading of libsnes library." no
add_command_line_string LIBSNES "libsnes library used" "-lsnes"
add_command_line_enable FFMPEG "Enable FFmpeg recording support" no
add_command_line_enable DYLIB "Enable dynamic loading support" auto
add_command_line_enable NETPLAY "Enable netplay support" auto
add_command_line_enable SRC "Enable libsamplerate support" no
add_command_line_enable CONFIGFILE "Disable support for config file" yes
add_command_line_enable CG "Enable Cg shader support" auto
add_command_line_enable XML "Enable bSNES-style XML shader support" auto
add_command_line_enable FBO "Enable render-to-texture (FBO) support" auto
add_command_line_enable ALSA "Enable ALSA support" auto
add_command_line_enable OSS "Enable OSS support" auto
add_command_line_enable RSOUND "Enable RSound support" auto
add_command_line_enable ROAR "Enable RoarAudio support" auto
add_command_line_enable AL "Enable OpenAL support" auto
add_command_line_enable JACK "Enable JACK support" auto
add_command_line_enable PULSE "Enable PulseAudio support" auto
add_command_line_enable FREETYPE "Enable FreeType support" auto
add_command_line_enable XVIDEO "Enable XVideo support" auto
add_command_line_enable SDL_IMAGE "Enable SDL_image support" auto
add_command_line_enable PYTHON "Enable Python 3 support for shaders" no
