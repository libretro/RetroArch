HAVE_FILE_LOGGER=1
MISSING_DECLS   =0

ifneq ($(C90_BUILD),)
   C89_BUILD=1
endif

include config.mk

TARGET = retroarch

OBJDIR := obj-unix

ifeq ($(GLOBAL_CONFIG_DIR),)
   GLOBAL_CONFIG_DIR = /etc
endif

OBJ := 
LIBS :=
DEFINES := -DHAVE_CONFIG_H -DRARCH_INTERNAL -DHAVE_OVERLAY
DEFINES += -DGLOBAL_CONFIG_DIR='"$(GLOBAL_CONFIG_DIR)"'

ifneq ($(findstring Win32,$(OS)),)
   LDFLAGS += -static-libgcc
endif

include Makefile.common

ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang"),1)
   DEFINES +=  -Wno-invalid-source-encoding
endif

HEADERS = $(wildcard */*/*.h) $(wildcard */*.h) $(wildcard *.h)

ifeq ($(MISSING_DECLS), 1)
	DEFINES += -Werror=missing-declarations
endif

ifeq ($(HAVE_DYLIB), 1)
   LIBS += $(DYLIB_LIB)
endif

ifeq ($(HAVE_DYNAMIC), 1)
   LIBS += $(DYLIB_LIB)
else
   LIBS += $(libretro)
endif

ifneq ($(V),1)
   Q := @
endif

ifeq ($(DEBUG), 1)
   OPTIMIZE_FLAG = -O0 -g
else
   OPTIMIZE_FLAG = -O3 -ffast-math
ifneq ($(findstring Win32,$(OS)),)
   LDFLAGS += -mwindows
endif
endif

CFLAGS   += -Wall $(OPTIMIZE_FLAG) $(INCLUDE_DIRS) $(DEBUG_FLAG) -I.
CXXFLAGS := $(CFLAGS) -std=c++98 -D__STDC_CONSTANT_MACROS
OBJCFLAGS :=  $(CFLAGS) -D__STDC_CONSTANT_MACROS

ifeq ($(CXX_BUILD), 1)
   LINK = $(CXX)
   CFLAGS := $(CXXFLAGS) -xc++
else
   ifeq ($(findstring Win32,$(OS)),)
      LINK = $(CC)
   else
      # directx-related code is c++
      LINK = $(CXX)
   endif

   ifneq ($(GNU90_BUILD), 1)
      ifneq ($(findstring icc,$(CC)),)
         CFLAGS += -std=c99 -D_GNU_SOURCE
      else
         CFLAGS += -std=gnu99 -D_GNU_SOURCE
      endif
   endif

   ifneq ($(C89_BUILD),)
      CFLAGS += -std=c89 -ansi -pedantic -Werror=pedantic -Wno-long-long
   endif
endif

ifeq ($(NOUNUSED), yes)
   CFLAGS += -Wno-unused-result
   CXXFLAGS += -Wno-unused-result
endif
ifeq ($(NOUNUSED_VARIABLE), yes)
   CFLAGS += -Wno-unused-variable
   CXXFLAGS += -Wno-unused-variable
endif

RARCH_OBJ := $(addprefix $(OBJDIR)/,$(OBJ))

ifneq ($(SANITIZER),)
    CFLAGS   := -fsanitize=$(SANITIZER) $(CFLAGS)
    CXXFLAGS := -fsanitize=$(SANITIZER) $(CXXFLAGS)
    LDFLAGS  := -fsanitize=$(SANITIZER) $(LDLAGS)
endif

ifneq ($(findstring $(GPERFTOOLS),profiler),)
   LIBS += -lprofiler
endif
ifneq ($(findstring $(GPERFTOOLS),tcmalloc),)
   LIBS += -ltcmalloc
endif

all: $(TARGET) config.mk

-include $(RARCH_OBJ:.o=.d)
config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

retroarch: $(RARCH_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LINK) -o $@ $(RARCH_OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

$(OBJDIR)/%.o: %.c config.h config.mk
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -c -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CXX $<),)
	$(Q)$(CXX) $(CXXFLAGS) $(DEFINES) -MMD -c -o $@ $<

$(OBJDIR)/%.o: %.m
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo OBJC $<),)
	$(Q)$(CXX) $(OBJCFLAGS) $(DEFINES) -MMD -c -o $@ $<

.FORCE:

$(OBJDIR)/git_version.o: git_version.c .FORCE
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -c -o $@ $<

$(OBJDIR)/%.o: %.S config.h config.mk $(HEADERS)
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo AS $<),)
	$(Q)$(CC) $(CFLAGS) $(ASFLAGS) $(DEFINES) -c -o $@ $<

$(OBJDIR)/%.o: %.rc $(HEADERS)
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo WINDRES $<),)
	$(Q)$(WINDRES) -o $@ $<

install: $(TARGET)
	rm -f $(OBJDIR)/git_version.o
	mkdir -p $(DESTDIR)$(PREFIX)/bin 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(GLOBAL_CONFIG_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(MAN_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/pixmaps 2>/dev/null || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin 
	install -m755 tools/cg2glsl.py $(DESTDIR)$(PREFIX)/bin/retroarch-cg2glsl
	install -m644 retroarch.cfg $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	install -m644 retroarch.desktop $(DESTDIR)$(PREFIX)/share/applications
	install -m644 docs/retroarch.1 $(DESTDIR)$(MAN_DIR)
	install -m644 docs/retroarch-cg2glsl.1 $(DESTDIR)$(MAN_DIR)
	install -m644 media/retroarch.svg $(DESTDIR)$(PREFIX)/share/pixmaps

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch-cg2glsl
	rm -f $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/applications/retroarch.desktop
	rm -f $(DESTDIR)$(MAN_DIR)/retroarch.1
	rm -f $(DESTDIR)$(MAN_DIR)/retroarch-cg2glsl.1
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/retroarch.svg

clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET)
	rm -f *.d

.PHONY: all install uninstall clean
