ifeq ($(platform),)
   platform = unix
   ifeq ($(shell uname -a),)
      platform = win
   else ifneq ($(findstring Darwin,$(shell uname -a)),)
      platform = osx
	arch = intel
   ifeq ($(shell uname -p),powerpc)
	   arch = ppc
	endif
   else ifneq ($(findstring MINGW,$(shell uname -a)),)
      platform = win
   else ifneq ($(findstring win,$(shell uname -a)),)
      platform = win
   endif
endif

ifeq ($(platform), osx)
_LATEST_SDK := $(shell xcrun -sdk iphoneos -show-sdk-version)
else
_SDK_DIR := $(THEOS)/sdks
_IOS_SDKS := $(sort $(patsubst $(_SDK_DIR)/iPhoneOS%.sdk,%,$(wildcard $(_SDK_DIR)/iPhoneOS*.sdk)))
_LATEST_SDK := $(word $(words $(_IOS_SDKS)),$(_IOS_SDKS))
endif

SDKVERSION := $(_LATEST_SDK)
ARCHS = armv7 armv7s
include $(THEOS)/makefiles/common.mk

SRC_DIR := ../..

APPLICATION_NAME = retroarch
retroarch_FILES = $(SRC_DIR)/griffin/griffin.c \
		  $(SRC_DIR)/audio/utils_neon.S \
		  $(SRC_DIR)/audio/resamplers/sinc_neon.S \
		  $(SRC_DIR)/audio/resamplers/cc_resampler_neon.S \
		  $(SRC_DIR)/apple/iOS/browser.m \
		  $(SRC_DIR)/apple/iOS/menu.m \
		  $(SRC_DIR)/apple/iOS/platform.m \
		  $(SRC_DIR)/apple/common/apple_gamecontroller.m \
	          $(SRC_DIR)/apple/common/CFExtensions.m \
		  $(SRC_DIR)/apple/common/RAGameView.m \
		  $(SRC_DIR)/apple/common/utility.m
retroarch_FRAMEWORKS = Foundation UIKit CoreGraphics AudioToolbox GLKit OpenGLES CoreText CoreLocation CoreAudio AVFoundation CoreMedia CoreVideo GameController

COMMON_FLAGS := -DIOS -DHAVE_GRIFFIN -DHAVE_LOCATION -DHAVE_RGUI -DHAVE_MENU -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_FBO -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_GLSL -DINLINE=inline -DLSB_FIRST -DHAVE_THREADS -D__LIBRETRO__ -DRARCH_MOBILE -std=gnu99 -DHAVE_COREAUDIO -DHAVE_FBO -DHAVE_OVERLAY -DHAVE_ZLIB -DWANT_MINIZ -DSINC_LOWER_QUALITY -DRARCH_INTERNAL -DHAVE_FILTERS_BUILTIN -DHAVE_LAKKA -DHAVE_XMB -D_LZMA_UINT32_IS_ULONG -DHAVE_STRL
COMMON_FLAGS += -DHAVE_CAMERA
COMMON_IOS_FLAGS := -Wno-deprecated-declarations -Wno-error
COMMON_IOS_OBJCFLAGS := -fobjc-arc 
INCFLAGS       := -I$(SRC_DIR) -I$(SRC_DIR)/libretro-sdk/include
retroarch_CFLAGS  += $(COMMON_FLAGS) $(COMMON_IOS_FLAGS) $(COMMON_IOS_OBJCFLAGS) $(INCFLAGS)
retroarch_CCFLAGS += $(COMMON_FLAGS) $(COMMON_IOS_FLAGS) $(COMMON_IOS_OBJCFLAGS) $(INCFLAGS)

include $(THEOS_MAKE_PATH)/application.mk
