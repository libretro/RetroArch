RARCH_VERSION		= "0.9.8-beta2"
LOCAL_PATH := $(call my-dir)
PERF_TEST := 0
HAVE_OPENSL     := 1

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM -marm
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS += -DANDROID_X86 -DHAVE_SSSE3
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
#LOCAL_CFLAGS += -mfpu=neon
LOCAL_CFLAGS += -DANDROID_ARM_V7
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_MODULE    := retroarch-activity

RARCH_PATH  := ../../..
LIBXML_PATH := ../libxml2
LOCAL_LIBXML_SRC_FILES = $(LIBXML_PATH)/c14n.c \
							$(LIBXML_PATH)/catalog.c \
							$(LIBXML_PATH)/chvalid.c \
							$(LIBXML_PATH)/debugXML.c \
							$(LIBXML_PATH)/dict.c \
							$(LIBXML_PATH)/DOCBparser.c \
							$(LIBXML_PATH)/encoding.c \
							$(LIBXML_PATH)/entities.c \
							$(LIBXML_PATH)/error.c \
							$(LIBXML_PATH)/globals.c \
							$(LIBXML_PATH)/hash.c \
							$(LIBXML_PATH)/legacy.c \
							$(LIBXML_PATH)/list.c \
							$(LIBXML_PATH)/parser.c \
							$(LIBXML_PATH)/parserInternals.c \
							$(LIBXML_PATH)/pattern.c \
							$(LIBXML_PATH)/relaxng.c \
							$(LIBXML_PATH)/SAX.c \
							$(LIBXML_PATH)/SAX2.c \
							$(LIBXML_PATH)/schematron.c \
							$(LIBXML_PATH)/threads.c \
							$(LIBXML_PATH)/tree.c \
							$(LIBXML_PATH)/uri.c \
							$(LIBXML_PATH)/valid.c \
							$(LIBXML_PATH)/xinclude.c \
							$(LIBXML_PATH)/xlink.c \
							$(LIBXML_PATH)/xmlIO.c \
							$(LIBXML_PATH)/xmlmemory.c \
							$(LIBXML_PATH)/xmlmodule.c \
							$(LIBXML_PATH)/xmlreader.c \
							$(LIBXML_PATH)/xmlregexp.c \
							$(LIBXML_PATH)/xmlsave.c \
							$(LIBXML_PATH)/xmlschemas.c \
							$(LIBXML_PATH)/xmlschemastypes.c \
							$(LIBXML_PATH)/xmlstring.c \
							$(LIBXML_PATH)/xmlunicode.c \
							$(LIBXML_PATH)/xmlwriter.c \
							$(LIBXML_PATH)/xpath.c \
							$(LIBXML_PATH)/xpointer.c
LOCAL_SRC_FILES    =	$(RARCH_PATH)/console/griffin/griffin.c $(LOCAL_LIBXML_SRC_FILES)


ifeq ($(PERF_TEST), 1)
LOCAL_CFLAGS += -DPERF_TEST
endif

LOCAL_CFLAGS += -O3 -fno-stack-protector -funroll-loops -DNDEBUG -DHAVE_GRIFFIN -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_OPENGLES -DHAVE_VID_CONTEXT -DHAVE_OPENGLES2 -DGLSL_DEBUG -DHAVE_GLSL -DHAVE_XML -DHAVE_ZLIB -DINLINE=inline -DLSB_FIRST -DHAVE_THREAD -D__LIBRETRO__ -DHAVE_CONFIGFILE=1 -DRARCH_PERFORMANCE_MODE -DRARCH_GPU_PERFORMANCE_MODE -DPACKAGE_VERSION=\"$(RARCH_VERSION)\" -std=gnu99

LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL -lGLESv2 -llog -ldl -lz
LOCAL_C_INCLUDES += $(LIBXML_PATH)

ifeq ($(HAVE_OPENSL), 1)
LOCAL_CFLAGS += -DHAVE_SL
LOCAL_LDLIBS += -lOpenSLES
endif

include $(BUILD_SHARED_LIBRARY)
