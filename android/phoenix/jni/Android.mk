LOCAL_PATH := $(call my-dir)
include $(LOCAL_PATH)/../../native/jni/Android.mk

# Include all present sub-modules
FOCAL_PATH := $(LOCAL_PATH)

define function
$(eval RETRO_MODULE_OBJECT := $(1))
$(eval include $(FOCAL_PATH)/modules/Android.mk)
endef

$(foreach m,$(wildcard $(FOCAL_PATH)/modules/*.so),$(eval $(call function,$(m))))
