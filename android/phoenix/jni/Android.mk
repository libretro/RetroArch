LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := retroarch-jni
RARCH_DIR := $(LOCAL_PATH)/../../..
LOCAL_CFLAGS += -std=gnu99 -Wall -DHAVE_LOGGER -DRARCH_DUMMY_LOG -DHAVE_ZLIB -DHAVE_MMAP -I$(RARCH_DIR)
LOCAL_LDLIBS := -llog -lz
LOCAL_SRC_FILES := apk-extract/apk-extract.c $(RARCH_DIR)/file_extract.c $(RARCH_DIR)/file_path.c $(RARCH_DIR)/compat/compat.c

include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/../../native/jni/Android.mk

