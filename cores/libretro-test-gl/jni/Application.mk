APP_ABI := all
ifeq ($(GLES), 3)
   APP_PLATFORM := android-18
else
   APP_PLATFORM := android-9
endif

