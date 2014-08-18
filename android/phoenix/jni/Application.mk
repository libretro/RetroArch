ifeq ($(GLES),3)
   APP_PLATFORM := $(NDK_GL_HEADER_VER)
else
   APP_PLATFORM := $(NDK_NO_GL_HEADER_VER)
endif
APP_ABI := armeabi-v7a mips x86
