TARGET := progress_bounds_test

# The bundled rcheevos runtime's progress reader against blobs held in
# buffers of exactly their size: run under ASan, a read past one fails.
#    make && make check
REPO_ROOT := ../../..
RC        := $(REPO_ROOT)/deps/rcheevos
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -std=gnu99 -g -O1 -I$(RC)/include -I$(LRC)/include -DRC_DISABLE_LUA

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring undefined,$(SANITIZER)),)
   CFLAGS  += -fno-sanitize-recover=undefined
   LDFLAGS += -fno-sanitize-recover=undefined
endif

SOURCES := progress_bounds_test.c \
   $(wildcard $(RC)/src/rcheevos/*.c) \
   $(RC)/src/rc_util.c \
   $(RC)/src/rc_compat.c \
   $(LRC)/utils/md5.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -lm

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
