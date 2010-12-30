. qb/qb.libs.sh

check_switch_c C99 -std=gnu99
check_critical C99 "Cannot find C99 compatible compiler."

check_lib_cxx SNES $LIBSNES snes_init -ldl
check_critical SNES "Cannot find libsnes."
add_define_make libsnes $LIBSNES

check_lib ALSA -lasound snd_pcm_open
check_header OSS sys/soundcard.h
check_lib AL -lopenal alcOpenDevice
check_lib RSOUND -lrsound rsd_init
check_lib ROAR -lroar roar_vs_new

check_lib GLFW -lglfw glfwInit
check_critical GLFW "Cannot find GLFW library."

check_lib CG -lCg cgCreateContext

check_lib SRC -lsamplerate src_callback_new

check_lib DYNAMIC -ldl dlopen

# Creates config.mk.
VARS="ALSA OSS AL RSOUND ROAR GLFW FILTER CG DYNAMIC"
create_config_make config.mk $VARS
create_config_header config.h $VARS

