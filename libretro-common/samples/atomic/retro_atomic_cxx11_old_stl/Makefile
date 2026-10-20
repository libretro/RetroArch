TARGET := retro_atomic_cxx11_old_stl_test

LIBRETRO_COMM_DIR := ../../..

SOURCE := retro_atomic_cxx11_old_stl_test.cpp

CXXFLAGS += -Wall -Wextra -pedantic -std=c++11 -g1 -O0 \
            -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CXXFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# old_stl/atomic is the pre-P0558R1 <atomic>; -I puts it ahead of the
# host's.  It declares and never defines, so the TU is only parsed.
all: $(TARGET) old-stl

$(TARGET): $(SOURCE)
	$(CXX) -o $@ $< $(CXXFLAGS) $(LDFLAGS)

old-stl: $(SOURCE)
	$(CXX) -fsyntax-only -Iold_stl $< $(CXXFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all old-stl check clean
