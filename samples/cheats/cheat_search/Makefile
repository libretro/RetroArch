include ../../sample_objdir.mk

TARGET := cheat_search_test

REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

SOURCES := cheat_search_test.c \
           $(REPO_ROOT)/cheat_manager.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/features/features_cpu.c

# cheat_manager.h reaches setting_list.h as ../setting_list.h
CFLAGS += -Wall -std=gnu99 -g1 -O0 -DRARCH_INTERNAL -DHAVE_CHEATS -DCHEAT_SCAN_TEST_HOOKS \
          -I$(REPO_ROOT) -I$(REPO_ROOT)/menu -I$(LRC)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean
