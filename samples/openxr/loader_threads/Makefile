# deps/openxr's loader under concurrent use, against a stand-in runtime
# (stub_runtime.c). Built from the loader's own sources; run it with
# SANITIZER=thread or SANITIZER=address,undefined.
TARGET   := loader_threads_test
ROOT     := ../../..
LRC      := $(ROOT)/libretro-common
XR       := $(ROOT)/deps/openxr

ifeq ($(OS),Windows_NT)
   EXE     := .exe
   STUB    := stub_runtime.dll
   XR_OS   := -DXR_OS_WINDOWS -DXR_USE_PLATFORM_WIN32
   LIBS    := -ladvapi32 -lshlwapi
   STUB_LD := -shared
else
   EXE     :=
   STUB    := libstub_runtime.so
   XR_OS   := -DXR_OS_LINUX
   LIBS    := -lpthread -ldl
   STUB_LD := -shared -fPIC
endif

INCLUDES := -I$(LRC)/include -I$(XR)/include -I$(XR)/src -I$(XR)/src/common -I$(XR)/src/loader
DEFINES  := -DHAVE_THREADS $(XR_OS)

LOADER_SOURCES := \
   $(XR)/src/common/object_info.cpp \
   $(XR)/src/common/filesystem_utils.cpp \
   $(XR)/src/loader/api_layer_interface.cpp \
   $(XR)/src/loader/loader_core.cpp \
   $(XR)/src/loader/loader_init_data.cpp \
   $(XR)/src/loader/loader_json.cpp \
   $(XR)/src/loader/loader_instance.cpp \
   $(XR)/src/loader/loader_logger.cpp \
   $(XR)/src/loader/loader_logger_recorders.cpp \
   $(XR)/src/loader/loader_properties.cpp \
   $(XR)/src/loader/manifest_file.cpp \
   $(XR)/src/loader/runtime_interface.cpp \
   $(XR)/src/loader/xr_generated_loader.cpp

C_SOURCES := \
   loader_threads_test.c \
   $(XR)/src/xr_generated_dispatch_table_core.c \
   $(LRC)/rthreads/rthreads.c \
   $(LRC)/formats/json/rjson.c \
   $(LRC)/file/file_path.c \
   $(LRC)/file/file_path_io.c \
   $(LRC)/file/retro_dirent.c \
   $(LRC)/vfs/vfs_implementation.c \
   $(LRC)/string/stdstring.c \
   $(LRC)/string/rstrtod.c \
   $(LRC)/compat/compat_strl.c \
   $(LRC)/compat/compat_strcasestr.c \
   $(LRC)/compat/fopen_utf8.c \
   $(LRC)/encodings/encoding_utf.c \
   $(LRC)/time/rtime.c

OBJS := $(C_SOURCES:.c=.o) $(LOADER_SOURCES:.cpp=.o)

CFLAGS   += -g -O1 -Wall $(DEFINES) $(INCLUDES)
CXXFLAGS += -g -O1 -Wall -std=gnu++17 -fno-exceptions $(DEFINES) $(INCLUDES)
LDFLAGS  +=

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   CXXFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE) $(STUB)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TARGET)$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

$(STUB): stub_runtime.c
	$(CC) -g -O1 -Wall -fvisibility=hidden -I$(XR)/include $(STUB_LD) -o $@ $<

check: all
	TSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
	   UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	   XR_LOADER_DEBUG=none ./$(TARGET)$(EXE) "$(CURDIR)"

clean:
	rm -f $(TARGET)$(EXE) $(STUB) $(OBJS) stub_runtime.json

.PHONY: all check clean
