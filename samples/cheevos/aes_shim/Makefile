TARGET := aes_shim_test

# cheevos/cheevos_aes.c against known blocks and a frozen reference corpus.
#    make && make check
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -std=gnu99 -g1 -O1 -I$(LRC)/include
CFLAGS += $(EXTRA_CFLAGS)

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring undefined,$(SANITIZER)),)
   CFLAGS  += -fno-sanitize-recover=undefined
   LDFLAGS += -fno-sanitize-recover=undefined
endif

SOURCES := aes_shim_test.c \
   $(REPO_ROOT)/cheevos/cheevos_aes.c \
   $(LRC)/crypto/crypto.c \
   $(LRC)/utils/md5.c \
   $(LRC)/features/features_cpu.c \
   $(LRC)/compat/compat_strl.c \
   $(LRC)/time/rtime.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -lm

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
