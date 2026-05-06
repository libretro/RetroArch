HAVE_FILE_LOGGER=1
HAVE_STATESTREAM?=1
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
DEF_FLAGS := -I.
ASFLAGS :=
DEFINES := -DHAVE_CONFIG_H -DRARCH_INTERNAL -D_FILE_OFFSET_BITS=64
DEFINES += -DGLOBAL_CONFIG_DIR='"$(GLOBAL_CONFIG_DIR)"'
DEFINES += -DASSETS_DIR='"$(DESTDIR)$(ASSETS_DIR)"'
DEFINES += -DFILTERS_DIR='"$(DESTDIR)$(FILTERS_DIR)"'
DEFINES += -DCORE_INFO_DIR='"$(DESTDIR)$(CORE_INFO_DIR)"'

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
endif

DEF_FLAGS += -Wall -Wsign-compare

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
   LDFLAGS += -static-libgcc -lwinmm -limm32
endif

include Makefile.common

ifeq ($(shell $(CC) -v 2>&1 | grep -c "clang"),1)
   DEF_FLAGS += -Wno-invalid-source-encoding -Wno-incompatible-ms-struct
endif

ifeq ($(shell $(CC) -v 2>&1 | grep -c "tcc"),1)
   MD = -MD
else
   MD = -MMD
endif

HEADERS = $(wildcard */*/*.h) $(wildcard */*.h) $(wildcard *.h)

ifeq ($(MISSING_DECLS), 1)
   DEF_FLAGS += -Werror=missing-declarations
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
      CFLAGS += -std=c89 -ansi -pedantic -Werror=pedantic -Wno-long-long -Werror=declaration-after-statement -Wno-variadic-macros
   else ifeq ($(HAVE_C99), 1)
      CFLAGS += $(C99_CFLAGS)
   endif

   CFLAGS += -D_GNU_SOURCE
endif

DEF_FLAGS += $(INCLUDE_DIRS) -Ideps -Ideps/stb

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

ifeq ($(HAVE_METAL), 1)
   METALLIB := default.metallib
endif

all: $(TARGET) $(METALLIB) config.mk

define INFO
ASFLAGS: $(ASFLAGS)
CC: $(CC)
CFLAGS: $(CFLAGS)
CPPFLAGS: $(CPPFLAGS)
CXX: $(CXX)
CXXFLAGS: $(CXXFLAGS)
DEFINES: $(DEFINES)
LDFLAGS: $(LDFLAGS)
LIBRARY_DIRS: $(LIBRARY_DIRS)
LIBS: $(LIBS)
LINK: $(LINK)
MD: $(MD)
MOC: $(MOC)
MOC_TMP: $(MOC_TMP)
OBJCFLAGS: $(OBJCFLAGS)
QT_VERSION: $(QT_VERSION)
RARCH_OBJ: $(RARCH_OBJ)
WINDRES: $(WINDRES)
endef
export INFO

info:
ifneq ($(V),1)
	@echo "$$INFO"
endif

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

# Compile the Metal shader library used by gfx/drivers/metal.m via
# [device newDefaultLibrary]. Xcode produces this automatically for the
# Metal.xcodeproj build; the commandline build has to do it by hand.
# The .metallib must sit next to the retroarch binary at runtime.
ifeq ($(HAVE_METAL), 1)
METAL_SHADER_SRCS := gfx/common/metal/Shaders.metal
METAL_AIR_FILES  := $(METAL_SHADER_SRCS:.metal=.air)

%.air: %.metal
	@$(if $(Q), $(shell echo echo METAL $<),)
	$(Q)xcrun -sdk macosx metal $(ARCHFLAGS) -c $< -o $@

default.metallib: $(METAL_AIR_FILES)
	@$(if $(Q), $(shell echo echo METALLIB $@),)
	$(Q)xcrun -sdk macosx metallib $(METAL_AIR_FILES) -o $@
endif

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

# ARC (Automatic Reference Counting) overrides. These Objective-C
# files use ARC-only constructs (__weak, __bridge*, no manual
# retain/release) and must be built with -fobjc-arc. The rest of the
# RetroArch Objective-C code is MRC-written (explicit retain/release,
# NSAutoreleasePool, etc.) and would fail to compile under ARC — so
# we cannot set -fobjc-arc globally. Xcode does the equivalent via
# per-file CLANG_ENABLE_OBJC_ARC=YES build settings.
$(OBJDIR)/gfx/drivers/metal.o: OBJCFLAGS += -fobjc-arc
$(OBJDIR)/input/drivers_joypad/mfi_joypad.o: OBJCFLAGS += -fobjc-arc
$(OBJDIR)/audio/drivers/coreaudio3.o: OBJCFLAGS += -fobjc-arc
$(OBJDIR)/input/drivers/cocoa_input.o: OBJCFLAGS += -fobjc-arc
$(OBJDIR)/location/drivers/corelocation.o: OBJCFLAGS += -fobjc-arc

$(OBJDIR)/%.o: %.S config.h config.mk $(HEADERS)
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo AS $<),)
	$(Q)$(CC) $(CFLAGS) $(ASFLAGS) $(DEFINES) -c -o $@ $<

$(OBJDIR)/%.o: %.rc $(HEADERS)
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo WINDRES $<),)
	$(Q)$(WINDRES) $(DEFINES) -o $@ $<

install: $(TARGET)
	mkdir -p $(DESTDIR)$(BIN_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(GLOBAL_CONFIG_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(DATA_DIR)/applications 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(DATA_DIR)/metainfo 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(DOC_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(MAN_DIR)/man6 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(DATA_DIR)/pixmaps 2>/dev/null || /bin/true
	cp $(TARGET) $(DESTDIR)$(BIN_DIR)
	@if test "$(HAVE_METAL)" = "1" && test -f default.metallib; then \
		cp default.metallib $(DESTDIR)$(BIN_DIR)/; \
	fi
	cp tools/cg2glsl.py $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	cp retroarch.cfg $(DESTDIR)$(GLOBAL_CONFIG_DIR)
	cp com.libretro.RetroArch.metainfo.xml $(DESTDIR)$(DATA_DIR)/metainfo
	cp com.libretro.RetroArch.desktop $(DESTDIR)$(DATA_DIR)/applications
	cp docs/retroarch.6 $(DESTDIR)$(MAN_DIR)/man6
	cp docs/retroarch-cg2glsl.6 $(DESTDIR)$(MAN_DIR)/man6
	cp media/com.libretro.RetroArch.svg $(DESTDIR)$(DATA_DIR)/pixmaps
	cp COPYING $(DESTDIR)$(DOC_DIR)
	cp README.md $(DESTDIR)$(DOC_DIR)
	chmod 755 $(DESTDIR)$(BIN_DIR)/$(TARGET)
	chmod 755 $(DESTDIR)$(BIN_DIR)/retroarch-cg2glsl
	chmod 644 $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	chmod 644 $(DESTDIR)$(DATA_DIR)/applications/com.libretro.RetroArch.desktop
	chmod 644 $(DESTDIR)$(DATA_DIR)/metainfo/com.libretro.RetroArch.metainfo.xml
	chmod 644 $(DESTDIR)$(MAN_DIR)/man6/retroarch.6
	chmod 644 $(DESTDIR)$(MAN_DIR)/man6/retroarch-cg2glsl.6
	chmod 644 $(DESTDIR)$(DATA_DIR)/pixmaps/com.libretro.RetroArch.svg
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
	rm -f $(DESTDIR)$(DATA_DIR)/applications/com.libretro.RetroArch.desktop
	rm -f $(DESTDIR)$(DATA_DIR)/metainfo/com.libretro.RetroArch.metainfo.xml
	rm -f $(DESTDIR)$(DATA_DIR)/pixmaps/com.libretro.RetroArch.svg
	rm -f $(DESTDIR)$(DOC_DIR)/COPYING
	rm -f $(DESTDIR)$(DOC_DIR)/COPYING.assets
	rm -f $(DESTDIR)$(DOC_DIR)/README.md
	rm -f $(DESTDIR)$(MAN_DIR)/man6/retroarch.6
	rm -f $(DESTDIR)$(MAN_DIR)/man6/retroarch-cg2glsl.6
	rm -rf $(DESTDIR)$(ASSETS_DIR)

clean:
	@$(if $(Q), echo $@,)
	$(Q)rm -rf $(OBJDIR_BASE)
	$(Q)rm -f $(TARGET)
	$(Q)rm -f *.d
	$(Q)rm -f default.metallib gfx/common/metal/*.air
	$(Q)rm -rf $(BUNDLE)

# ---------------------------------------------------------------------------
# make bundle — assemble RetroArch.app from the build outputs (macOS only).
#
# Mirrors what pkg/apple/RetroArch_Metal.xcodeproj produces, minus code
# signing, entitlements, asset archives, and framework-wrapped cores. The
# resulting .app is a plain, ad-hoc bundle suitable for local testing and
# for distribution outside the App Store.
#
# Invoked as `make bundle`. Not part of `all:`; plain `make` builds only
# the retroarch binary and default.metallib exactly as before.
#
# The deployment target in Info.plist (LSMinimumSystemVersion) is derived
# from the -mmacosx-version-min=X.Y flag the binary was linked with (set
# earlier in this Makefile via $(MINVERFLAGS) based on the target arch).
# Build floors per Makefile.common:130-164:
#    arm64        -> 10.15  (Apple Silicon)
#    x86_64+Metal -> 10.13
#    x86_64       -> 10.7
#    i386         -> 10.6
#    powerpc      -> 10.5
# To target a lower OS version than the default for the current arch,
# cross-compile with ARCH=<arch> (e.g. `ARCH=ppc make && make bundle`)
# or override BUNDLE_MIN_OS directly.
#
# User-overridable variables:
#    BUNDLE            bundle directory name           (default: RetroArch.app)
#    BUNDLE_EXECUTABLE binary name inside Contents/MacOS (default: RetroArch)
#    BUNDLE_IDENTIFIER CFBundleIdentifier              (default: com.libretro.dist.RetroArch)
#    BUNDLE_VERSION    CFBundleShortVersionString      (default: from version.all)
#    BUNDLE_BUILD      CFBundleVersion                 (default: 44)
#    BUNDLE_MIN_OS     LSMinimumSystemVersion          (default: derived from MINVERFLAGS)
#
# Example: BUNDLE=RetroArchDev.app BUNDLE_IDENTIFIER=com.example.dev make bundle
# ---------------------------------------------------------------------------

ifneq ($(findstring Darwin,$(OS)),)

# version.all is a C/Make/shell polyglot, but all lines start with '#' so
# '-include version.all' is a no-op from Make's perspective. Parse the
# #define out via shell instead.
PACKAGE_VERSION    := $(shell grep 'define PACKAGE_VERSION' version.all | cut -d'"' -f2)

BUNDLE             ?= RetroArch.app
BUNDLE_EXECUTABLE  ?= RetroArch
BUNDLE_IDENTIFIER  ?= com.libretro.dist.RetroArch
BUNDLE_VERSION     ?= $(PACKAGE_VERSION)
BUNDLE_BUILD       ?= 44
# Extract X.Y from '-mmacosx-version-min=X.Y' inside $(MINVERFLAGS).
# If nothing matches (e.g. building with a custom toolchain), fall back to 10.13.
BUNDLE_MIN_OS      ?= $(or $(patsubst -mmacosx-version-min=%,%,$(filter -mmacosx-version-min=%,$(MINVERFLAGS))),10.13)
# Detect legacy macOS targets (< 10.9 Mavericks). Pre-Mavericks codesign
# predates the `--timestamp` option; the dyld/Gatekeeper enforcement that
# makes ad-hoc signing worth doing didn't exist yet either, so on legacy
# targets we skip signing entirely rather than fight an older toolchain.
# We key off BUNDLE_MIN_OS rather than the build host so cross-compiling
# for ppc/10.5 on a modern Mac still picks up the legacy path.
MACOS_LEGACY       := $(shell echo $(BUNDLE_MIN_OS) | awk -F. '{ exit !($$1 < 10 || ($$1 == 10 && $$2 < 9)) }' && echo 1)
INFO_PLIST_SRC     := pkg/apple/OSX/Info_Metal.plist
# Universal (arm64 + x86_64) MoltenVK.framework shipped in the repo.
# Only copied when HAVE_VULKAN=1; on pre-Metal / non-Vulkan builds
# (e.g. ppc 10.5, i386 10.6) the Vulkan code path isn't compiled in
# and shipping MoltenVK would be dead weight that can't even load.
MOLTENVK_FRAMEWORK := pkg/apple/Frameworks/MoltenVK.xcframework/macos-arm64_x86_64/MoltenVK.framework

bundle: $(TARGET) $(METALLIB)
	@echo "Assembling $(BUNDLE) (min macOS $(BUNDLE_MIN_OS))"
	$(Q)rm -rf $(BUNDLE)
	$(Q)mkdir -p $(BUNDLE)/Contents/MacOS
	$(Q)mkdir -p $(BUNDLE)/Contents/Resources/filters/audio
	$(Q)mkdir -p $(BUNDLE)/Contents/Resources/filters/video
	$(Q)cp $(TARGET) $(BUNDLE)/Contents/MacOS/$(BUNDLE_EXECUTABLE)
	$(Q)chmod +x $(BUNDLE)/Contents/MacOS/$(BUNDLE_EXECUTABLE)
	$(Q)if [ -f default.metallib ]; then \
		cp default.metallib $(BUNDLE)/Contents/Resources/default.metallib; \
	elif [ -f pkg/apple/OSX/Resources/default.metallib ]; then \
		cp pkg/apple/OSX/Resources/default.metallib $(BUNDLE)/Contents/Resources/default.metallib; \
	fi
	$(Q)cp libretro-common/audio/dsp_filters/*.dsp \
		$(BUNDLE)/Contents/Resources/filters/audio/ 2>/dev/null || true
	$(Q)cp gfx/video_filters/*.filt \
		$(BUNDLE)/Contents/Resources/filters/video/ 2>/dev/null || true
	$(Q)if [ "$(HAVE_VULKAN)" = "1" ] && [ -d $(MOLTENVK_FRAMEWORK) ]; then \
		mkdir -p $(BUNDLE)/Contents/Frameworks; \
		cp -R $(MOLTENVK_FRAMEWORK) $(BUNDLE)/Contents/Frameworks/; \
	fi
	$(Q)printf 'APPL????' > $(BUNDLE)/Contents/PkgInfo
	$(Q)sed \
		-e 's|$$(EXECUTABLE_NAME)|$(BUNDLE_EXECUTABLE)|g' \
		-e 's|$${PRODUCT_NAME}|$(BUNDLE_EXECUTABLE)|g' \
		-e 's|$$(PRODUCT_BUNDLE_IDENTIFIER)|$(BUNDLE_IDENTIFIER)|g' \
		-e 's|$$(MARKETING_VERSION)|$(BUNDLE_VERSION)|g' \
		-e 's|$$(CURRENT_PROJECT_VERSION)|$(BUNDLE_BUILD)|g' \
		-e 's|$$(MACOSX_DEPLOYMENT_TARGET)|$(BUNDLE_MIN_OS)|g' \
		$(INFO_PLIST_SRC) > $(BUNDLE)/Contents/Info.plist
	@# Ad-hoc code signing. On Apple Silicon (and increasingly on Intel
	@# with hardened runtime enforcement), dyld refuses to load unsigned
	@# dylibs even for ad-hoc app-internal use — including the MoltenVK
	@# framework copied in above. `codesign --sign -` produces an ad-hoc
	@# signature that satisfies the loader without needing a developer
	@# identity. Sign nested content first (frameworks), then the outer
	@# .app wrapper, so the app's seal covers all contents.
	@#
	@# Skip entirely on pre-Mavericks targets: those toolchains predate
	@# `--timestamp`, the dyld enforcement that makes this necessary, and
	@# in some cases ad-hoc signing support altogether.
	$(Q)if [ "$(MACOS_LEGACY)" != "1" ]; then \
		if [ -d $(BUNDLE)/Contents/Frameworks ]; then \
			for fw in $(BUNDLE)/Contents/Frameworks/*.framework; do \
				[ -d "$$fw" ] && codesign --force --sign - --timestamp=none "$$fw"; \
			done; \
		fi; \
		codesign --force --sign - --timestamp=none $(BUNDLE); \
	fi
	@echo "Done. Run with: open $(BUNDLE)"

.PHONY: bundle

endif

.PHONY: all install uninstall clean

print-%:
	@echo '$*=$($*)'
