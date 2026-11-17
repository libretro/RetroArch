TARGET      := retro_atomic_inline_override_test
TARGET_CXX  := retro_atomic_inline_override_test_cxx98
TARGET_CXX11 := retro_atomic_inline_override_test_cxx11

LIBRETRO_COMM_DIR := ../../..

# One source three ways: as C89, and as C++98 and C++11 (the C++11
# backend of retro_atomic.h). The source defines INLINE as FBNeo does
# before any libretro-common header; a header that spells its inline
# functions "static INLINE" fails the C++ builds.
SOURCE := retro_atomic_inline_override_test.c

CFLAGS   += -Wall -pedantic -std=c89 -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include
CXXFLAGS += -Wall -pedantic -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   CXXFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_CXX) $(TARGET_CXX11)

$(TARGET): $(SOURCE)
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

$(TARGET_CXX): $(SOURCE)
	$(CXX) -x c++ -std=c++98 -o $@ $< $(CXXFLAGS) $(LDFLAGS)

$(TARGET_CXX11): $(SOURCE)
	$(CXX) -x c++ -std=c++11 -o $@ $< $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_CXX) $(TARGET_CXX11)

.PHONY: all clean
