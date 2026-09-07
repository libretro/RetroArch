TARGET      := retro_atomic_extern_c_linkage_test
TARGET_TEST := retro_atomic_extern_c_linkage_test_cxx

LIBRETRO_COMM_DIR := ../../..

# Two binaries from one source.  The source mirrors ui_qt.cpp's pattern
# of wrapping a RetroArch header include in extern "C" { ... }, gated
# on !CXX_BUILD.  We build it once each way:
#
#   $(TARGET)      : non-unity / separate-TU build (extern "C" wrap is
#                    active).  This is the configuration that triggered
#                    the original "template with C linkage" failure on
#                    g++ before the retro_common_api.h fix.
#
#   $(TARGET_TEST) : unity / griffin build (CXX_BUILD defined, extern
#                    "C" wrappers compile out).  Validates that the
#                    fix did not regress the unity-build path.
#
# Both must compile and pass.  Build failure on $(TARGET) is the
# regression signal for the original bug.

SOURCE := retro_atomic_extern_c_linkage_test.cpp

# C++11 is the minimum that engages retro_atomic.h's CXX11 backend.
# -Wall -Wextra to catch any incidental warning the test's macro
# expansions might produce on a future toolchain.
CXXFLAGS += -Wall -Wextra -pedantic -std=c++11 -g -O0 \
            -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CXXFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# The unity-build target gets CXX_BUILD defined, exactly as
# Makefile.griffin does for griffin_cpp.cpp's TUs.
$(TARGET_TEST): CXXFLAGS += -DCXX_BUILD

all: $(TARGET) $(TARGET_TEST)

$(TARGET): $(SOURCE)
	$(CXX) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(TARGET_TEST): $(SOURCE)
	$(CXX) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST)

.PHONY: all clean
