ROOT := ../../..
LRC := $(ROOT)/libretro-common
CFLAGS += -std=gnu89 -O1 -g -Wall -Wextra -Wno-unused-function \
          -Wno-unused-parameter -ffunction-sections -fdata-sections \
          -DRARCH_INTERNAL -DHAVE_OPENXR \
          -I$(ROOT) -I$(LRC)/include -I$(ROOT)/deps \
          -I$(ROOT)/deps/openxr/include
LDFLAGS += -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: openxr_haptics_test
openxr_haptics_test: openxr_haptics_test.c $(ROOT)/input/common/input_openxr.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ openxr_haptics_test.c \
	   $(LRC)/compat/compat_strl.c -lm
check: all
	./openxr_haptics_test
clean:
	rm -f openxr_haptics_test openxr_haptics_test.exe
.PHONY: all check clean
