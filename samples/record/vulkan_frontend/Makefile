include ../../sample_objdir.mk
.DEFAULT_GOAL := all
CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wno-c99-extensions -Werror=declaration-after-statement -Wframe-larger-than=2048 \
   -DHAVE_RECORD_VULKAN -DHAVE_THREADS -DHAVE_CONFIGFILE -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
CFLAGS += -ffunction-sections -fdata-sections -MMD -MP
SOURCES := frontend_test.c $(RA)/libretro-common/file/config_file.c $(RA)/libretro-common/file/file_path.c \
   $(RA)/libretro-common/string/stdstring.c $(RA)/libretro-common/string/rstrtod.c \
   $(RA)/libretro-common/compat/compat_strl.c $(RA)/libretro-common/compat/compat_strldup.c \
   $(RA)/libretro-common/compat/compat_posix_string.c $(RA)/libretro-common/encodings/encoding_utf.c \
   $(RA)/libretro-common/time/rtime.c
OBJS := $(call sample_objs,$(SOURCES:.c=.o))
# Existing configuration dependencies use the project's GNU89 dialect.
$(filter-out frontend_test.o,$(OBJS)): CFLAGS := $(filter-out -std=c89 -pedantic,$(CFLAGS)) -std=gnu89 -Werror=declaration-after-statement -Wno-unused-parameter
ifeq ($(EXE),.exe)
$(call sample_objs,$(RA)/libretro-common/time/rtime.o): CFLAGS += -Wno-cast-function-type
endif
-include $(OBJS:.o=.d)
all: frontend_test$(EXE)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
frontend_test$(EXE): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS) -Wl,--gc-sections -lm
check: all
	$(RUNNER) ./frontend_test$(EXE)
clean:
	rm -f frontend_stream_test.exe
	rm -f frontend_test frontend_test.exe
	rm -f frontend_test.o frontend_test.d
	rm -rf $(SAMPLE_OBJDIR)
.PHONY: all check clean

ifeq ($(EXE),.exe)
all: frontend_stream_test$(EXE)
check: stream-check
frontend_stream_test$(EXE): frontend_test.c $(RA)/record/drivers/record_vulkan.c $(filter-out frontend_test.o,$(OBJS))
	$(CC) $(CFLAGS) -DHAVE_NETWORKING -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -DHAVE_CRYPTO $< $(filter-out frontend_test.o,$(OBJS)) -o $@ $(LDFLAGS) -Wl,--gc-sections -lm
stream-check: frontend_stream_test$(EXE)
	$(RUNNER) ./frontend_stream_test$(EXE)
.PHONY: stream-check
endif

check: platform-check
platform-check:
	python3 platform_test.py
.PHONY: platform-check
