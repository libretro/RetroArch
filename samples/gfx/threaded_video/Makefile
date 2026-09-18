# See build.sh: this harness links the shipping RetroArch objects with
# only main() replaced, so it needs the project's own build rather than
# a source list of its own.
#
# SAN= selects which tree build to link, as the suite passes it:
#   SAN=address (default)  -> make SANITIZER=address,undefined DEBUG=1
#   SAN=thread             -> make SANITIZER=thread DEBUG=1
#   SAN=none               -> plain release build
# The matching tree build must exist first; build.sh says so if not.
TARGET := threaded_video_test
SAN    ?= address
CYCLES ?= 10

ifeq ($(SAN),address)
MAKE_ARGS := SANITIZER=address,undefined DEBUG=1
else ifeq ($(SAN),thread)
MAKE_ARGS := SANITIZER=thread DEBUG=1
else
MAKE_ARGS :=
endif
# VK_DEBUG=1 links a tree built with VULKAN_DEBUG=1 (validation layer
# on), which check-vulkan wants.
ifeq ($(VK_DEBUG),1)
MAKE_ARGS += VULKAN_DEBUG=1
endif
# GFX_INSTRUMENT=1 here builds the harness against a tree built the
# same way: the surface and overlay lanes then print and check what
# the image paths cost. The tree must carry the flag too - the
# harness compiles with the tree's own compile line, but links the
# tree's objects - so it is passed in rather than forced:
#    make GFX_INSTRUMENT=1 && make -C samples/gfx/threaded_video \
#         GFX_INSTRUMENT=1 check
ifeq ($(GFX_INSTRUMENT),1)
MAKE_ARGS += GFX_INSTRUMENT=1
endif

all:
	@MAKE_ARGS="$(MAKE_ARGS)" ./build.sh

check: all
	@ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
	 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
	 TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
	 ./$(TARGET) $(CYCLES) > check.log 2>&1; rc=$$?; \
	 grep -vE '^\[(INFO|DEBUG|WARN|ERROR)\]' check.log; rm -f check.log; \
	 exit $$rc

# The same lanes on the Vulkan driver: a real context on whatever ICD
# VK_DRIVER_FILES names (lavapipe in CI), with the tree built
# VULKAN_DEBUG=1 so the validation layer is loaded and reports through
# the frontend log. Any validation error fails the run, and so does a
# harness failure. Needs an X display; with none the target skips
# (exit 77) rather than failing, as the vulkan_validation sample does
# - except on Windows (OS=Windows_NT, set by the system), where there
# is no X and the desktop is always there.
# Under a sanitizer (SAN=thread or SAN=address, matching the tree's
# build) the uninstrumented layer and ICD are suppressed through
# tsan-vulkan.supp / ubsan-vulkan.supp; frontend reports still fail.
check-vulkan: all
	@if [ -z "$$DISPLAY" ] && [ "$${OS:-}" != Windows_NT ]; then echo "check-vulkan: no DISPLAY, skipping"; exit 77; fi; \
	 HARNESS_VIDEO_DRIVER=vulkan HARNESS_VERBOSE=1 \
	 ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
	 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:suppressions=ubsan-vulkan.supp \
	 TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:suppressions=tsan-vulkan.supp \
	 ./$(TARGET) $(CYCLES) > check.log 2>&1; rc=$$?; \
	 grep -E '\[pass\]|\[baseline\]|\[info\]|\[skip\]|FAIL|all lanes|Validation Error' check.log | grep -v 'XFree86-VidMode'; \
	 if [ $$rc -ne 0 ]; then grep -vE '^\[(INFO|DEBUG|WARN)\]|XFree86-VidMode' check.log; fi; \
	 errs=$$(grep -c 'Validation Error' check.log); \
	 rm -f check.log; \
	 if [ "$$errs" != "0" ]; then echo "check-vulkan: $$errs validation error(s)"; exit 1; fi; \
	 exit $$rc

# The same lanes on any real driver named in DRIVER (gl, glcore, ...),
# through whatever context the display offers - in CI, llvmpipe under
# Xvfb with LIBGL_ALWAYS_SOFTWARE=1. No validation layer here: a
# harness failure, a sanitizer report or a crash is what fails it.
# GDB=1 runs the harness under gdb, so a crash on a platform with no
# core dumps to read afterwards (the Windows runner) still leaves its
# stacks in the log. The exit status is the harness's own.
# The faulting instruction and the symbol it belongs to, before the
# backtraces: a stack says which function stopped, not which access
# did, and a release build on a runner is the only place some of these
# crashes happen.
RUNNER := $(if $(filter 1,$(GDB)),gdb -q -batch -return-child-result -ex run -ex 'printf "--- fault site ---\n"' -ex 'x/i $$pc' -ex 'info symbol $$pc' -ex 'info registers rax rbx rcx rdx rsi rdi' -ex 'x/4i $$pc-12' -ex 'thread apply all bt 25' --args,)

check-driver: all
	@if [ -z "$$DISPLAY" ] && [ "$${OS:-}" != Windows_NT ]; then echo "check-driver: no DISPLAY, skipping"; exit 77; fi; \
	 if [ -z "$(DRIVER)" ]; then echo "check-driver: DRIVER=<video driver> required"; exit 2; fi; \
	 HARNESS_VIDEO_DRIVER=$(DRIVER) HARNESS_VERBOSE=1 \
	 ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
	 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:suppressions=ubsan-vulkan.supp \
	 TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:suppressions=tsan-gl.supp \
	 $(RUNNER) ./$(TARGET) $(CYCLES) > check.log 2>&1; rc=$$?; \
	 grep -E '\[pass\]|FAIL|all lanes' check.log | grep -v 'XFree86-VidMode'; \
	 if [ $$rc -ne 0 ]; then \
	    grep -vE '^\[(INFO|DEBUG|WARN)\]|XFree86-VidMode' check.log; \
	    echo "--- last 60 lines of the run log ---"; tail -60 check.log; \
	 fi; \
	 rm -f check.log; \
	 exit $$rc

clean:
	rm -f $(TARGET) check.log harness_core.so retroarch_nomain.o retroarch_nomain.d \
	      harness_main.o harness_main.d

.PHONY: all check check-vulkan check-driver clean
