# See build.sh: this harness links the shipping RetroArch objects with
# only main() replaced, so it needs the project's own build first.
#
# SAN= selects which tree build to link:
#   SAN=none (default)     -> plain release build; the heap lane needs
#                             this, an allocator sanitizer replaces
#                             malloc and would be counted
#   SAN=address            -> make SANITIZER=address,undefined DEBUG=1
#   SAN=thread             -> make SANITIZER=thread DEBUG=1
TARGET := frontend_overhead_test
SAN    ?= none

ifeq ($(SAN),address)
MAKE_ARGS := SANITIZER=address,undefined DEBUG=1
else ifeq ($(SAN),thread)
MAKE_ARGS := SANITIZER=thread DEBUG=1
else
MAKE_ARGS :=
endif

all:
	@MAKE_ARGS="$(MAKE_ARGS)" ./build.sh

check: all
	@ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
	 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	 TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
	 ./$(TARGET) > check.log 2>&1; rc=$$?; \
	 grep -vE '^\[(INFO|DEBUG|WARN|ERROR)\]' check.log; rm -f check.log; \
	 exit $$rc

clean:
	rm -f $(TARGET) check.log harness_core.so retroarch_nomain.o retroarch_nomain.d \
	      harness_main.o harness_main.d

.PHONY: all check clean
