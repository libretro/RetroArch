MAKE_ARGS ?=
EXE ?=

all:
	@MAKE_ARGS="$(MAKE_ARGS)" ./build.sh

check: all
	$(RUNNER) ./command_frames_test$(EXE) ./harness_core.so

clean:
	rm -f command_frames_test command_frames_test.exe harness_core.so
	rm -f retroarch_nomain.o retroarch_nomain.d harness_main.o harness_main.d

.PHONY: all check clean
