DEPLOYMENT_IOSVERSION = 5.0
TARGET = iphone:latest:$(DEPLOYMENT_IOSVERSION)
ARCHS = armv7 armv7s
TARGET_IPHONEOS_DEPLOYMENT_VERSION=$(DEPLOYMENT_IOSVERSION)
include $(THEOS)/makefiles/common.mk

SRC_DIR := ../..

APPLICATION_NAME = RetroArch
${APPLICATION_NAME}_FRAMEWORKS = Foundation UIKit CoreGraphics AudioToolbox GLKit OpenGLES CoreText CoreLocation CoreAudio AVFoundation CoreMedia CoreVideo GameController
${APPLICATION_NAME}_FILES = $(SRC_DIR)/griffin/griffin.c \
		  $(SRC_DIR)/audio/audio_utils_neon.S \
		  $(SRC_DIR)/audio/drivers_resampler/sinc_neon.S \
		  $(SRC_DIR)/audio/drivers_resampler/cc_resampler_neon.S \
		  $(SRC_DIR)/apple/iOS/browser.m \
		  $(SRC_DIR)/apple/iOS/menu.m \
		  $(SRC_DIR)/apple/iOS/platform.m \
		  $(SRC_DIR)/apple/common/apple_gamecontroller.m \
		  $(SRC_DIR)/apple/common/CFExtensions.m \
		  $(SRC_DIR)/apple/common/RAGameView.m \
		  $(SRC_DIR)/apple/common/utility.m \

COMMON_FLAGS := -DIOS -DHAVE_GRIFFIN -DHAVE_LOCATION -DHAVE_RGUI -DHAVE_MENU -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_FBO -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_GLSL -DINLINE=inline -DLSB_FIRST -DHAVE_THREADS -D__LIBRETRO__ -DRARCH_MOBILE -std=gnu99 -DHAVE_COREAUDIO -DHAVE_FBO -DHAVE_OVERLAY -DHAVE_ZLIB -DWANT_MINIZ -DSINC_LOWER_QUALITY -DRARCH_INTERNAL -DHAVE_FILTERS_BUILTIN -DHAVE_LAKKA -DHAVE_XMB -D_LZMA_UINT32_IS_ULONG -DHAVE_STRL
COMMON_FLAGS += -DHAVE_CAMERA
COMMON_IOS_FLAGS := -Wno-deprecated-declarations -Wno-error
COMMON_IOS_OBJCFLAGS := -fobjc-arc 
INCFLAGS       := -I$(SRC_DIR) -I$(SRC_DIR)/libretro-sdk/include
${APPLICATION_NAME}_CFLAGS  += $(COMMON_FLAGS) $(COMMON_IOS_FLAGS) $(COMMON_IOS_OBJCFLAGS) $(INCFLAGS)
${APPLICATION_NAME}_CCFLAGS += $(COMMON_FLAGS) $(COMMON_IOS_FLAGS) $(COMMON_IOS_OBJCFLAGS) $(INCFLAGS)

include $(THEOS_MAKE_PATH)/application.mk
