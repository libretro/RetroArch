# Choose LTO options that works with the chosen compiler. Also, the
# `ar` link archiver needs to be chosen as they are very specific to that
# version.
COMPILER_VERSION := $(shell $(CXX) --version 2>/dev/null)
ifneq (,$(findstring clang,$(COMPILER_VERSION)))
  LTO_FLAGS=-flto=thin
  AR=llvm-ar
else
  LTO_FLAGS=-flto=2
  AR=gcc-ar
endif

# When cross-compiling for aarch64 (e.g. using aarch64-linux-gnu-g++),
# the compiler does not accept '-march=native' or '-mtune=native'. Detect
# that situation and avoid those flags. For native builds keep the flags.
ifneq (,$(findstring aarch64-linux-gnu,$(CXX)))
  CPU_ARCH_FLAGS=
else
  CPU_ARCH_FLAGS=-march=native -mtune=native
endif
