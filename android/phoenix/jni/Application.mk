ifeq ($(GLES),3)
   APP_PLATFORM := android-18
else
   APP_PLATFORM := android-9
endif

ifndef TARGET_ABIS
   APP_ABI := armeabi-v7a mips x86
else
   APP_ABI := $(TARGET_ABIS)
endif
