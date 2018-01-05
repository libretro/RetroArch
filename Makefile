HAVE_FILE_LOGGER=1
HAVE_CC_RESAMPLER=1
NEED_CXX_LINKER=0
MISSING_DECLS   =0

ifneq ($(C90_BUILD),)
   C89_BUILD=1
endif

include config.mk

# Put your favorite compile flags in this file, if you want different defaults than upstream.
# Do not attempt to create that file upstream.
# (It'd be better to put this comment in that file, but .gitignore doesn't work on files that exist in the repo.)
-include Makefile.local

TARGET = retroarch

OBJDIR := obj-unix

OBJ :=
LIBS :=
DEFINES := -DHAVE_CONFIG_H -DRARCH_INTERNAL
DEFINES += -DGLOBAL_CONFIG_DIR='"$(GLOBAL_CONFIG_DIR)"'

ifneq ($(findstring BSD,$(OS)),)
   CFLAGS += -DBSD
   LDFLAGS += -L/usr/local/lib
endif

ifneq ($(findstring DOS,$(OS)),)
   CFLAGS += -march=i386
   LDFLAGS += -lemu
endif

ifneq ($(findstring Win32,$(OS)),)
   LDFLAGS += -static-libgcc -lwinmm
endif

include Makefile.common

ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang"),1)
   DEFINES +=  -Wno-invalid-source-encoding
endif

ifeq ($(shell $(CC) -v 2>&1 | grep -c "tcc"),1)
   MD = -MD
else
   MD = -MMD
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
endif

ifneq ($(findstring Win32,$(OS)),)
   LDFLAGS += -mwindows
endif

CFLAGS   += -Wall $(OPTIMIZE_FLAG) $(INCLUDE_DIRS) -I. -Ideps -Ideps/stb

APPEND_CFLAGS := $(CFLAGS)
CXXFLAGS += $(APPEND_CFLAGS) -std=c++11 -D__STDC_CONSTANT_MACROS
OBJCFLAGS :=  $(CFLAGS) -D__STDC_CONSTANT_MACROS

ifeq ($(CXX_BUILD), 1)
   LINK = $(CXX)
   CFLAGS   := $(CXXFLAGS) -xc++
   CFLAGS   += -DCXX_BUILD
   CXXFLAGS += -DCXX_BUILD
else
   ifeq ($(NEED_CXX_LINKER),1)
      LINK = $(CXX)
   else ifeq ($(findstring Win32,$(OS)),)
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

ifneq ($(X86),)
   CFLAGS += -m32
   CXXLAGS += -m32
   LDFLAGS += -m32
endif

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) $(CFLAGS)
   CXXFLAGS := -fsanitize=$(SANITIZER) $(CXXFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifneq ($(findstring $(GPERFTOOLS),profiler),)
   LIBS += -lprofiler
endif
ifneq ($(findstring $(GPERFTOOLS),tcmalloc),)
   LIBS += -ltcmalloc
endif

all: $(TARGET) config.mk

ifeq ($(MAKECMDGOALS),clean)
config.mk:
else
-include $(RARCH_OBJ:.o=.d)
config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1
endif

retroarch: $(RARCH_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LINK) -o $@ $(RARCH_OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

$(OBJDIR)/%.o: %.c config.h config.mk
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFINES) $(MD) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp config.h config.mk
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CXX $<),)
	$(Q)$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEFINES) -MMD -c -o $@ $<

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
	mkdir -p $(DESTDIR)$(BIN_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(GLOBAL_CONFIG_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(MAN_DIR)/man6 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/pixmaps 2>/dev/null || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(BIN_DIR)
	install -m755 tools/cg2glsl.py $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	install -m644 retroarch.cfg $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	install -m644 retroarch.desktop $(DESTDIR)$(PREFIX)/share/applications
	install -m644 docs/retroarch.6 $(DESTDIR)$(MAN_DIR)/man6
	install -m644 docs/retroarch-cg2glsl.6 $(DESTDIR)$(MAN_DIR)/man6
	install -m644 media/retroarch.svg $(DESTDIR)$(PREFIX)/share/pixmaps
	@if test -d media/assets; then \
		echo "Installing media assets..."; \
		mkdir -p $(DESTDIR)$(ASSETS_DIR)/retroarch/assets/xmb; \
		mkdir -p $(DESTDIR)$(ASSETS_DIR)/retroarch/assets/glui; \
		cp -r media/assets/xmb/  $(DESTDIR)$(ASSETS_DIR)/retroarch/assets; \
		cp -r media/assets/glui/ $(DESTDIR)$(ASSETS_DIR)/retroarch/assets; \
		echo "Asset copying done."; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BIN_DIR)/retroarch
	rm -f $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	rm -f $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/applications/retroarch.desktop
	rm -f $(DESTDIR)$(MAN_DIR)/man6/retroarch.6
	rm -f $(DESTDIR)$(MAN_DIR)/man6/retroarch-cg2glsl.6
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/retroarch.svg
	rm -rf $(DESTDIR)$(ASSETS_DIR)/retroarch

clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET)
	rm -f *.d

.PHONY: all install uninstall clean
