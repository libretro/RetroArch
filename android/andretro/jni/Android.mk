LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_CPP_FEATURES := exceptions rtti

LOCAL_SRC_FILES = Driver.cpp

LOCAL_MODULE := retroiface
LOCAL_LDLIBS := -llog -lz -lGLESv2

include $(BUILD_SHARED_LIBRARY)

# Include all present sub-modules
FOCAL_PATH := $(LOCAL_PATH)

define function
$(eval RETRO_MODULE_OBJECT := $(1))
$(eval include $(FOCAL_PATH)/modules/Android.mk)
endef

$(foreach m,$(wildcard $(FOCAL_PATH)/modules/*.so),$(eval $(call function,$(m))))
