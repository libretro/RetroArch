ifeq ($(GLES),3)
   ifndef NDK_GL_HEADER_VER
      APP_PLATFORM := android-18
   else
      APP_PLATFORM := $(NDK_GL_HEADER_VER)
   endif
else
   ifndef NDK_NO_GL_HEADER_VER
      APP_PLATFORM := android-9
   else
      APP_PLATFORM := $(NDK_NO_GL_HEADER_VER)
   endif
endif

ifndef TARGET_ABIS
   APP_ABI := armeabi-v7a mips x86
else
   APP_ABI := $(TARGET_ABIS)
endif
