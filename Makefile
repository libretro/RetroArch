HAVE_FILE_LOGGER=1
NEED_CXX_LINKER?=0
NEED_GOLD_LINKER?=0
MISSING_DECLS   =0

ifneq ($(C90_BUILD),)
   C89_BUILD=1
endif

include config.mk

# Put your favorite compile flags in this file, if you want different defaults than upstream.
# Do not attempt to create that file upstream.
# (It'd be better to put this comment in that file, but .gitignore doesn't work on files that exist in the repo.)
-include Makefile.local

ifeq ($(HAVE_ANGLE), 1)
TARGET = retroarch_angle
else
TARGET = retroarch
endif

OBJ :=
LIBS :=
DEF_FLAGS :=
ASFLAGS :=
DEFINES := -DHAVE_CONFIG_H -DRARCH_INTERNAL -D_FILE_OFFSET_BITS=64
DEFINES += -DGLOBAL_CONFIG_DIR='"$(GLOBAL_CONFIG_DIR)"'

OBJDIR_BASE := obj-unix

ifeq ($(NEED_GOLD_LINKER), 1)
   LDFLAGS += -fuse-ld=gold
endif

ifeq ($(DEBUG), 1)
   OBJDIR := $(OBJDIR_BASE)/debug
   CFLAGS ?= -O0 -g
   CXXFLAGS ?= -O0 -g
   DEFINES += -DDEBUG -D_DEBUG
else
   OBJDIR := $(OBJDIR_BASE)/release
   CFLAGS ?= -O3
   CXXFLAGS ?= -O3
   DEF_FLAGS += -ffast-math
endif

ifneq ($(findstring BSD,$(OS)),)
   DEF_FLAGS += -DBSD
   LDFLAGS += -L/usr/local/lib
   UDEV_CFLAGS += -I/usr/local/include/libepoll-shim
   UDEV_LIBS += -lepoll-shim
endif

ifneq ($(findstring DOS,$(OS)),)
   DEF_FLAGS += -march=i386
   LDFLAGS += -lemu
endif

ifneq ($(findstring FPGA,$(OS)),)
   DEFINES += -DHAVE_FPGA
endif

ifneq ($(findstring Win32,$(OS)),)
   LDFLAGS += -static-libgcc -lwinmm
endif

include Makefile.common

ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang"),1)
   DEFINES +=  -Wno-invalid-source-encoding -Wno-incompatible-ms-struct
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

ifeq ($(HAVE_DRMINGW), 1)
   DEF_FLAGS += -DHAVE_DRMINGW
   LDFLAGS += $(DRMINGW_LIBS)
endif

ifneq ($(findstring Win32,$(OS)),)
   LDFLAGS += -mwindows
endif

ifneq ($(CXX_BUILD), 1)
   ifneq ($(C89_BUILD),)
      CFLAGS += -std=c89 -ansi -pedantic -Werror=pedantic -Wno-long-long
   else ifeq ($(HAVE_C99), 1)
      CFLAGS += $(C99_CFLAGS)
   endif

   CFLAGS += -D_GNU_SOURCE
endif

DEF_FLAGS += -Wall $(INCLUDE_DIRS) -I. -Ideps -Ideps/stb

CFLAGS += $(DEF_FLAGS)
CXXFLAGS += $(DEF_FLAGS) -D__STDC_CONSTANT_MACROS
OBJCFLAGS :=  $(CFLAGS) -D__STDC_CONSTANT_MACROS

ifeq ($(HAVE_CXX), 1)
   ifeq ($(CXX_BUILD), 1)
      LINK = $(CXX)
      CFLAGS   := $(CXXFLAGS) -xc++
      CFLAGS   += -DCXX_BUILD
      CXXFLAGS += -DCXX_BUILD
   else ifeq ($(NEED_CXX_LINKER),1)
      LINK = $(CXX)
   else
      LINK = $(CC)
   endif
else
   LINK = $(CC)
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
   CXXFLAGS += -m32
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

# Qt MOC generation, required for QObject-derived classes
ifneq ($(MOC_HEADERS),)
    # prefix moc_ to base filename of paths and change extension from h to cpp, so a/b/foo.h becomes a/b/moc_foo.cpp
    MOC_SRC := $(join $(addsuffix moc_,$(addprefix $(OBJDIR)/,$(dir $(MOC_HEADERS)))), $(notdir $(MOC_HEADERS:.h=.cpp)))
    MOC_OBJ := $(patsubst %.cpp,%.o,$(MOC_SRC))
    RARCH_OBJ += $(MOC_OBJ)
endif

all: $(TARGET) config.mk

$(MOC_SRC):
	@$(if $(Q), $(shell echo echo MOC $<),)
	$(eval MOC_TMP := $(patsubst %.h,%_moc.cpp,$@))
	$(Q)QT_SELECT=$(QT_VERSION) $(MOC) -o $(MOC_TMP) $<

$(foreach x,$(join $(addsuffix :,$(MOC_SRC)),$(MOC_HEADERS)),$(eval $x))

$(MOC_OBJ):
	@$(if $(Q), $(shell echo echo CXX $<),)
	$(Q)$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEFINES) -MMD -c -o $@ $<

$(foreach x,$(join $(addsuffix :,$(MOC_OBJ)),$(MOC_SRC)),$(eval $x))

ifeq ($(MAKECMDGOALS),clean)
config.mk:
else
-include $(RARCH_OBJ:.o=.d)
ifeq ($(HAVE_CONFIG_MK),)
config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1
endif
endif

SYMBOL_MAP := -Wl,-Map=output.map

$(TARGET): $(RARCH_OBJ)
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
	mkdir -p $(DESTDIR)$(DATA_DIR)/applications 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(DOC_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(MAN_DIR)/man6 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(DATA_DIR)/pixmaps 2>/dev/null || /bin/true
	cp $(TARGET) $(DESTDIR)$(BIN_DIR)
	cp tools/cg2glsl.py $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	cp retroarch.cfg $(DESTDIR)$(GLOBAL_CONFIG_DIR)
	cp retroarch.desktop $(DESTDIR)$(DATA_DIR)/applications
	cp docs/retroarch.6 $(DESTDIR)$(MAN_DIR)/man6
	cp docs/retroarch-cg2glsl.6 $(DESTDIR)$(MAN_DIR)/man6
	cp media/retroarch.svg $(DESTDIR)$(DATA_DIR)/pixmaps
	cp COPYING $(DESTDIR)$(DOC_DIR)
	cp README.md $(DESTDIR)$(DOC_DIR)
	chmod 755 $(DESTDIR)$(BIN_DIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	chmod 644 $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	chmod 644 $(DESTDIR)$(DATA_DIR)/applications/retroarch.desktop
	chmod 644 $(DESTDIR)$(MAN_DIR)/man6/retroarch.6
	chmod 644 $(DESTDIR)$(MAN_DIR)/man6/retroarch-cg2glsl.6
	chmod 644 $(DESTDIR)$(DATA_DIR)/pixmaps/retroarch.svg
	@if test -d media/assets && test $(HAVE_ASSETS); then \
		echo "Installing media assets..."; \
		mkdir -p $(DESTDIR)$(ASSETS_DIR)/assets; \
		if test $(HAVE_MATERIALUI) = 1; then \
			cp -r media/assets/glui/ $(DESTDIR)$(ASSETS_DIR)/assets; \
		fi; \
		if test $(HAVE_XMB) = 1; then \
			cp -r media/assets/xmb/ $(DESTDIR)$(ASSETS_DIR)/assets; \
		fi; \
		if test $(HAVE_OZONE) = 1; then \
			cp -r media/assets/ozone/ $(DESTDIR)$(ASSETS_DIR)/assets; \
		fi; \
		cp media/assets/COPYING $(DESTDIR)$(DOC_DIR)/COPYING.assets; \
		echo "Asset copying done."; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BIN_DIR)/$(TARGET)
	rm -f $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	rm -f $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	rm -f $(DESTDIR)$(DATA_DIR)/applications/retroarch.desktop
	rm -f $(DESTDIR)$(DATA_DIR)/pixmaps/retroarch.svg
	rm -f $(DESTDIR)$(DOC_DIR)/COPYING
	rm -f $(DESTDIR)$(DOC_DIR)/COPYING.assets
	rm -f $(DESTDIR)$(DOC_DIR)/README.md
	rm -f $(DESTDIR)$(MAN_DIR)/man6/retroarch.6
	rm -f $(DESTDIR)$(MAN_DIR)/man6/retroarch-cg2glsl.6
	rm -rf $(DESTDIR)$(ASSETS_DIR)

clean:
	rm -rf $(OBJDIR_BASE)
	rm -f $(TARGET)
	rm -f *.d

.PHONY: all install uninstall clean

print-%:
	@echo '$*=$($*)'
