# The link harness and the link test core. Everything built, and every
# run's output, goes under BUILD_DIR. check runs each scenario six times:
# first, again, in reversed pick order, in a shuffled one, from the first
# run's mid state and rewound to it, each later run's trace, messages.log
# and stuck.txt compared against the first run's. The from run must start
# at the scenario's save mid line and not run that line again. The rewound
# run must give the first run's trace and messages up to the scenario's
# last frame, then from the save mid line on, and the first run's
# checkpoint pictures. A scenario must save before its last frame, or the
# rewound run would load where it saved. Where the scenario restarts its
# ticks at a load, both runs' messages.log are compared without them.
# Where it has a compare-fresh line, the frames a run makes before it loads
# a state are compared in the trace fields that line names; in host_stamp,
# which leaves S out, the again run's S fields must not be the first run's,
# its from run must fail a first trace with an S field edited, and two runs
# with no state must fail one with a V or an A field edited.
# An in_flight checkpoint, at the save frame, must list a message. In
# spans_waiting and spans_membership, whose late-join step runs again,
# console 0 must finish one frame a step; console 1 of spans_waiting must be
# parked at the checkpoint named parked, and the checkpoints of
# spans_membership must find consoles 1 and 2 inside a frame or at an edge
# as its header says. In two_spans console 1 must finish 598 frames to
# console 0's 600, and in spans_save_in_advance and render_spans consoles 1
# and 2 must be parked at their checkpoints in_flight and last, and so must
# those of warm_save_in_advance, gl/spans and gl/mixed. Each of
# render_one's 120 frames must have a picture of its own, and render_grow's
# picture must be 64 wide at its checkpoint small and 128 at mid; console 1
# of render_two must keep its picture of frame 99 over the frame after it,
# which passes none, and render_dupe's console its picture of frame 39.
# Then each scenario in bad/ must exit 2 and each in fail/ exit 1, printing
# the error its "# error:" line names, as must a truncated state, a state
# of another scenario and an edited trace; each scenario in stale/ must
# pass its from run and fail its rewound run in the same way, and each in
# cold/, whose core loads no state before its first frame, pass its first
# run and fail its from run. warm_load is that scenario with warm=1, which
# runs a frame of the console before the from run's load.
#
# A console line's render=gl needs a harness built with HAVE_EGL=1, and
# render=vulkan one built with HAVE_VULKAN=1. While a switch is off, its
# scenario in unbuilt/ must exit 2 as those in bad/ do. No object is built
# again when a switch, CC or SANITIZER changes: each set of them takes a
# BUILD_DIR of its own.
#
# check-gl, which check ends with when HAVE_EGL is 1, makes the six runs of
# each scenario in gl/ on surfaceless EGL contexts. Such a scenario's first
# line, "# twin: <name>", names the scenario it differs from only in where
# its pictures are drawn: its first run's trace, messages and checkpoint
# pictures must be the twin's first run's. Each scenario in gl/fail/ must
# exit 1 as those in fail/ do. GPU_DRIVER=software, the default, runs them
# on Mesa's llvmpipe and fails a console whose renderer is another, which
# one more run must show failing; GPU_DRIVER=host runs them on the
# machine's own driver.
#
# A console's OpenGL context is made current each time its code runs again,
# in a frame or out of one: gl/held unloads console 0's core, which looks
# for its own context then, after console 1 alone has run. gl/two, whose
# consoles park between the two parts of their pictures, must fail its
# first run's trace when run with --no-context-follow, which leaves the
# last context made current for both, and one more run of it must count
# more than 400 changes of context. On llvmpipe gl/gles2 runs once more in
# a context Mesa holds to OpenGL ES 2.0, where it gives a later one
# otherwise.
#
# proof makes the six runs of one scenario that binds real cores, PROOF
# (an absolute path), into BUILD_DIR/proof/<scenario>/, each BIND word
# (NAME=<absolute file>, no spaces) a --path.

LRC := ../../../libretro-common
BUILD_DIR ?= build
ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR is empty)
endif
ifneq ($(filter / . ./,$(strip $(BUILD_DIR))),)
$(error BUILD_DIR must be a folder of its own, not "$(strip $(BUILD_DIR))")
endif
ifneq ($(filter ..,$(subst /, ,$(BUILD_DIR))),)
$(error BUILD_DIR must not climb out with .., not "$(strip $(BUILD_DIR))")
endif

HAVE_EGL    ?= 0
HAVE_VULKAN ?= 0
GPU_DRIVER  ?= software
HARNESS_DEFINES :=
HARNESS_HEADERS := link_scenario.h link_console.h
GPU_LIBS :=

TRIPLE := $(shell $(CC) -dumpmachine)
EXE :=
SO := .so
SHARED := -shared -fPIC
PIC := -fPIC
DL_LIBS := -ldl
ifneq ($(findstring mingw,$(TRIPLE)),)
EXE := .exe
SO := .dll
SHARED := -shared
PIC :=
DL_LIBS :=
else ifeq ($(shell uname -s),Darwin)
SO := .dylib
SHARED := -dynamiclib -fPIC
endif

HARNESS := $(BUILD_DIR)/link_harness$(EXE)
CORE    := $(BUILD_DIR)/link_test_libretro$(SO)
OBJ_DIR := $(BUILD_DIR)/obj
obj = $(addprefix $(OBJ_DIR)/,$(subst ../,__/,$(1:.c=.o)))

HARNESS_SOURCES := link_harness.c link_scenario.c link_console.c
LINK_SOURCES    := $(LRC)/link/link_bus.c $(LRC)/link/link_sched.c
LIBCO_SOURCES   := $(LRC)/libco/libco.c
LRC_SOURCES     := \
	$(LRC)/dynamic/dylib.c \
	$(LRC)/encodings/encoding_utf.c \
	$(LRC)/encodings/encoding_xxh64.c \
	$(LRC)/compat/fopen_utf8.c \
	$(LRC)/compat/compat_strl.c \
	$(LRC)/compat/compat_posix_string.c \
	$(LRC)/compat/compat_strcasestr.c \
	$(LRC)/file/file_path.c \
	$(LRC)/file/file_path_io.c \
	$(LRC)/file/rpng_file.c \
	$(LRC)/formats/png/rpng_encode.c \
	$(LRC)/encodings/encoding_crc32.c \
	$(LRC)/encodings/encoding_deflate.c \
	$(LRC)/features/features_cpu.c \
	$(LRC)/memory/memdesc.c \
	$(LRC)/streams/interface_stream.c \
	$(LRC)/streams/memory_stream.c \
	$(LRC)/streams/trans_stream.c \
	$(LRC)/streams/trans_stream_pipe.c \
	$(LRC)/streams/trans_stream_deflate.c \
	$(LRC)/streams/file_stream.c \
	$(LRC)/string/rstrtod.c \
	$(LRC)/vfs/vfs_implementation.c \
	$(LRC)/time/rtime.c
CORE_SOURCES    := ../core/link_test_core.c ../core/link_test_gl.c
SCENARIOS       := $(sort $(wildcard ../scenarios/*.scn))
BAD_SCENARIOS   := $(sort $(wildcard ../scenarios/bad/*.scn))
ifeq ($(HAVE_EGL),1)
ifneq ($(findstring mingw,$(TRIPLE)),)
$(error HAVE_EGL=1 needs EGL, which a mingw build does not have)
endif
HARNESS_SOURCES += link_gl.c
HARNESS_HEADERS += link_gl.h
HARNESS_DEFINES += -DHAVE_EGL -DEGL_NO_X11
GPU_LIBS        += -lEGL
else
BAD_SCENARIOS   += ../scenarios/unbuilt/gl.scn
endif
ifeq ($(HAVE_VULKAN),1)
HARNESS_DEFINES += -DHAVE_VULKAN
else
BAD_SCENARIOS   += ../scenarios/unbuilt/vulkan.scn
endif
FAIL_SCENARIOS  := $(sort $(wildcard ../scenarios/fail/*.scn))
UNTICK          := s/ stamp [0-9a-f]* landing [0-9a-f]*//
ALL_RUNS        := first again reversed shuffled from rewound

# What goes in front of a run with a GPU console, and that run's arguments
ifeq ($(GPU_DRIVER),software)
GPU_ENV  := env -u DISPLAY -u WAYLAND_DISPLAY LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
GPU_ARGS := --require-renderer llvmpipe
else ifeq ($(GPU_DRIVER),host)
GPU_ENV  := env -u DISPLAY -u WAYLAND_DISPLAY
GPU_ARGS :=
else
$(error GPU_DRIVER takes software or host, not "$(GPU_DRIVER)")
endif

# After a run that must fail, named $$what: it exited $(1), and its stderr,
# in $$err, has $$why
FAILED_AS = rc=$$?; cat "$$err"; \
	[ $$rc -eq $(1) ] || { echo "check: $$what exited $$rc, not $(1)"; exit 1; }; \
	grep -F -q -e "$$why" "$$err" || { echo "check: $$what did not say: $$why"; exit 1; }

# The first run's lines, in the file named after it, that a rewound run
# repeats: those before the frame it loads at, then those from the save on
REWOUND = awk -v to="$$last" -v from="$$save" \
	'NR == FNR { if ($$2 + 0 < to + 0) print; next } $$2 + 0 >= from + 0'

# The runs $(3) of the scenario in $$scn, into $(1)/<its name>/<run>/,
# with $(2) binding its names and $(4) in front of each; the first failure
# stops them
RUNS = name=`basename "$$scn" .scn`; out="$(1)/$$name"; \
	save=`sed -n 's/^\([0-9][0-9]*\) save mid\( .*\)*$$/\1/p' "$$scn"`; \
	[ -n "$$save" ] || { echo "$@: $$name has no save mid line"; exit 1; }; \
	last=`sed -n 's/^expect-stuck \([0-9][0-9]*\)\( .*\)*$$/\1/p' "$$scn"`; \
	[ -n "$$last" ] || last=`sed -n 's/^\([0-9][0-9]*\) end\( .*\)*$$/\1/p' "$$scn"`; \
	[ "$$save" -lt "$$last" ] || { echo "$@: $$name saves at or after its last frame"; exit 1; }; \
	flight=`sed -n 's/^\([0-9][0-9]*\) checkpoint in_flight\( .*\)*$$/\1/p' "$$scn"`; \
	[ -z "$$flight" ] || [ "$$flight" = "$$save" ] \
	   || { echo "$@: $$name's in_flight checkpoint is not at its save frame"; exit 1; }; \
	untick=; grep -q '^option [0-9][0-9]* link_test_restart_ticks on\( .*\)*$$' "$$scn" \
	   && untick='$(UNTICK)'; \
	fields=`sed -n 's/^compare-fresh \([VAS]*\)\( .*\)*$$/\1/p' "$$scn"`; \
	for run in $(3); do \
	   case $$run in \
	      first)    args="--out $$out/first" ;; \
	      again)    args="--out $$out/again --expect $$out/first/trace.txt" ;; \
	      reversed) args="--pick reversed --out $$out/reversed --expect $$out/first/trace.txt" ;; \
	      shuffled) args="--pick shuffled:7 --out $$out/shuffled --expect $$out/first/trace.txt" ;; \
	      from)     args="--from $$out/first/mid.state --out $$out/from --expect $$out/first/trace.txt"; \
	                rm -f -- "$$out/from/mid.state" ;; \
	      rewound)  args="--rewind $$out/first/mid.state --out $$out/rewound --expect $$out/first/trace.txt" ;; \
	   esac; \
	   rm -f -- "$$out/$$run/stuck.txt"; \
	   echo "== $$name $$run"; \
	   $(4) $(RUNNER) $(HARNESS) $(MIN_STACK) --scenario "$$scn" $(2) $$args \
	      || { echo "$@: $$name $$run failed with exit $$?"; exit 1; }; \
	   case $$run in \
	      first) ;; \
	      from) start=`sed -n '1s/^F \([0-9]*\) .*/\1/p' "$$out/from/trace.txt"`; \
	         [ "$$start" = "$$save" ] || { echo "$@: $$name from started at frame $$start, not $$save"; exit 1; }; \
	         [ ! -e "$$out/from/mid.state" ] || { echo "$@: $$name from ran its save line again"; exit 1; }; \
	         want=`awk -v from="$$save" '$$2 + 0 >= from + 0' "$$out/first/messages.log" \
	            | sed "$$untick" | cksum`; \
	         got=`sed "$$untick" "$$out/from/messages.log" | cksum`; \
	         [ "$$want" = "$$got" ] ;; \
	      rewound) fresh=; \
	         if [ -n "$$fields" ]; then \
	            for f in V A S; do \
	               case $$fields in *$$f*) ;; *) fresh="$$fresh s/ $$f [0-9a-f]*//;" ;; esac; \
	            done; \
	            n=`awk -v to="$$last" '$$2 + 0 < to + 0 { n++ } END { print n + 0 }' "$$out/first/trace.txt"`; \
	            fresh="1,$${n}{$$fresh }"; \
	         fi; \
	         $(REWOUND) "$$out/first/trace.txt" "$$out/first/trace.txt" | sed "$$fresh" \
	            > "$$out/rewound/want.txt"; \
	         sed "$$fresh" "$$out/rewound/trace.txt" | cmp -s "$$out/rewound/want.txt" - \
	            || { echo "$@: $$name rewound: trace.txt is not the first run's to frame $$last, then from frame $$save"; exit 1; }; \
	         want=`$(REWOUND) "$$out/first/messages.log" "$$out/first/messages.log" \
	            | sed "$$untick" | cksum`; \
	         got=`sed "$$untick" "$$out/rewound/messages.log" | cksum`; \
	         ( cd "$$out/first" && for png in */c*.png; do \
	            [ ! -e "$$png" ] || cmp -s "$$png" "../rewound/$$png" \
	               || { echo "$@: $$name rewound: $$png differs from the first run's"; exit 1; }; \
	         done ) || exit 1; \
	         [ "$$want" = "$$got" ] ;; \
	      *) cmp -s "$$out/first/messages.log" "$$out/$$run/messages.log" ;; \
	   esac || { echo "$@: $$name $$run: messages.log differs from the first run's"; exit 1; }; \
	   [ ! -e "$$out/first/stuck.txt" ] || cmp -s "$$out/first/stuck.txt" "$$out/$$run/stuck.txt" \
	      || { echo "$@: $$name $$run: stuck.txt differs from the first run's"; exit 1; }; \
	   [ -z "$$flight" ] || grep -q '^msg ' "$$out/$$run/in_flight/bus.txt" \
	      || { echo "$@: $$name $$run: no message in flight at the save"; exit 1; }; \
	done

# Each scenario of $(1) must fail, with $(2) in front of its run and $(3)
# among its arguments: one in bad/ or unbuilt/ with exit 2 and any other
# with exit 1, saying what its "# error:" line names
define MUST_FAIL
for scn in $(1); do \
	   what="$${scn#../scenarios/}"; what="$${what%.scn}"; \
	   kind=`dirname "$$what"`; kind=`basename "$$kind"`; \
	   case $$kind in bad|unbuilt) want=2 ;; *) want=1 ;; esac; \
	   err="$(BUILD_DIR)/out/$$what.err"; \
	   why=`sed -n 's/^# error: //p' "$$scn"`; \
	   [ -n "$$why" ] || { echo "$@: $$what has no # error: line"; exit 1; }; \
	   mkdir -p "$(BUILD_DIR)/out/`dirname "$$what"`"; \
	   echo "== $$what, exit $$want wanted"; \
	   $(2) $(RUNNER) $(HARNESS) --scenario "$$scn" --path test=$(CORE) $(3) \
	      --out "$(BUILD_DIR)/out/$$what" 2>"$$err"; \
	   $(call FAILED_AS,$$want); \
	done
endef

# Each scenario of ../scenarios/$(1)/ passes its runs $(2); its $(3) run,
# which loads the first run's state with $(4), must then exit 1, saying what
# its "# error:" line names
define LOAD_FAILS
	@for scn in $(sort $(wildcard ../scenarios/$(1)/*.scn)); do \
	   $(call RUNS,$(BUILD_DIR)/out/$(1),--path test=$(CORE),$(2)); \
	   what="$(1)/$$name $(3)"; err="$$out/$(3).err"; \
	   why=`sed -n 's/^# error: //p' "$$scn"`; \
	   [ -n "$$why" ] || { echo "$@: $$what has no # error: line"; exit 1; }; \
	   echo "== $$what, exit 1 wanted"; \
	   $(RUNNER) $(HARNESS) $(MIN_STACK) --scenario "$$scn" --path test=$(CORE) \
	      $(4) "$$out/first/mid.state" --expect "$$out/first/trace.txt" \
	      --out "$$out/$(3)" 2>"$$err"; \
	   $(call FAILED_AS,1); \
	done
endef

# Consoles 1 and 2 of each scenario of $(1), which saves them inside an
# advance, are parked at its checkpoints in_flight and last
define PARKED
for name in $(1); do \
	   for at in in_flight last; do \
	      for c in 1 2; do \
	         grep -q "^c$$c frame [0-9]* in_frame 1$$" \
	            "$(BUILD_DIR)/out/$$name/first/$$at/consoles.txt" \
	            || { echo "$@: $$name: console $$c is not parked at checkpoint $$at"; exit 1; }; \
	      done; \
	   done; \
	done
endef

# The scenarios of ../scenarios/$(1)/, which have GPU consoles, with $(2)
# among each run's arguments; those of $(3) must be PARKED
define GPU_CHECK
	@for scn in $(sort $(wildcard ../scenarios/$(1)/*.scn)); do \
	   twin=`sed -n '1s/^# twin: //p' "$$scn"`; \
	   [ -n "$$twin" ] && [ -f "../scenarios/$$twin.scn" ] \
	      || { echo "$@: $$scn names no twin on its first line"; exit 1; }; \
	   $(call RUNS,$(BUILD_DIR)/out/$(1),--path test=$(CORE) $(2),$(ALL_RUNS),$(GPU_ENV)); \
	   gpu="$$out/first"; what="$(1)/$$name"; soft="$(BUILD_DIR)/out/$$twin/first"; \
	   if [ ! -e "$$soft/trace.txt" ]; then \
	      scn="../scenarios/$$twin.scn"; $(call RUNS,$(BUILD_DIR)/out,--path test=$(CORE),first); \
	   fi; \
	   for f in trace.txt messages.log; do \
	      cmp -s "$$soft/$$f" "$$gpu/$$f" \
	         || { echo "$@: $$what: $$f differs from $$twin's"; exit 1; }; \
	   done; \
	   n=0; \
	   for png in "$$soft"/*/c*.png; do \
	      [ -e "$$png" ] || continue; \
	      n=$$((n + 1)); at="$${png#$$soft/}"; \
	      cmp -s "$$png" "$$gpu/$$at" \
	         || { echo "$@: $$what: $$at differs from $$twin's"; exit 1; }; \
	   done; \
	   m=`ls "$$gpu"/*/c*.png 2>/dev/null | wc -l`; \
	   [ "$$n" -gt 0 ] && [ "$$m" -eq "$$n" ] \
	      || { echo "$@: $$what: $$m checkpoint pictures to $$twin's $$n"; exit 1; }; \
	   [ "$(GPU_DRIVER)" = software ] || continue; \
	   for txt in "$$gpu"/c*/gpu.txt; do \
	      grep -q llvmpipe "$$txt" 2>/dev/null \
	         || { echo "$@: $$what: $$txt does not name llvmpipe"; exit 1; }; \
	   done; \
	done
	@$(call PARKED,$(3))
	@$(call MUST_FAIL,$(sort $(wildcard ../scenarios/$(1)/fail/*.scn)),$(GPU_ENV),$(2))
	@scn="$(firstword $(sort $(wildcard ../scenarios/$(1)/*.scn)))"; \
	what="$(1)/fail/renderer"; err="$(BUILD_DIR)/out/$$what.err"; \
	why="is not no-such-renderer"; \
	mkdir -p "$(BUILD_DIR)/out/$(1)/fail"; \
	echo "== $$scn --require-renderer no-such-renderer, exit 1 wanted"; \
	$(GPU_ENV) $(RUNNER) $(HARNESS) --scenario "$$scn" --path test=$(CORE) \
	   --require-renderer no-such-renderer --out "$(BUILD_DIR)/out/$$what" 2>"$$err"; \
	$(call FAILED_AS,1)
endef

# What comes after $(GPU_ENV) in a run that GL_CHECK makes
GL_RUN = $(RUNNER) $(HARNESS) $(MIN_STACK) --path test=$(CORE) $(GPU_ARGS)

# What only the OpenGL consoles are checked for, after GPU_CHECK
define GL_CHECK
	@out="$(BUILD_DIR)/out/gl"; what="gl/two --no-context-follow"; \
	err="$$out/fail/no_follow.err"; why="trace differs"; \
	mkdir -p "$$out/fail"; \
	echo "== $$what, exit 1 wanted"; \
	$(GPU_ENV) $(GL_RUN) --scenario ../scenarios/gl/two.scn --no-context-follow \
	   --expect "$$out/two/first/trace.txt" --out "$$out/fail/no_follow" 2>"$$err"; \
	$(call FAILED_AS,1)
	@out="$(BUILD_DIR)/out/gl/two"; \
	echo "== two counted"; \
	$(GPU_ENV) $(GL_RUN) --scenario ../scenarios/gl/two.scn \
	   --expect "$$out/first/trace.txt" --out "$$out/counted" > "$$out/counted.txt" \
	   || { echo "$@: gl/two counted failed with exit $$?"; exit 1; }; \
	cat "$$out/counted.txt"; \
	n=`sed -n 's/^link_harness: \([0-9][0-9]*\) GL context switches$$/\1/p' "$$out/counted.txt"`; \
	[ -n "$$n" ] && [ "$$n" -gt 400 ] \
	   || { echo "$@: gl/two: \"$$n\" GL context switches, not more than 400"; exit 1; }
	@[ "$(GPU_DRIVER)" = software ] || exit 0; \
	out="$(BUILD_DIR)/out/gl/gles2"; \
	echo "== gles2 es20"; \
	$(GPU_ENV) MESA_GLES_VERSION_OVERRIDE=2.0 $(GL_RUN) --scenario ../scenarios/gl/gles2.scn \
	   --expect "$$out/first/trace.txt" --out "$$out/es20" \
	   || { echo "$@: gl/gles2 es20 failed with exit $$?"; exit 1; }; \
	grep -q '^gl OpenGL ES 2\.0 ' "$$out/es20/c0/gpu.txt" \
	   || { echo "$@: gl/gles2 es20: not an OpenGL ES 2.0 context: `cat "$$out/es20/c0/gpu.txt"`"; exit 1; }
endef

BASE_CFLAGS  := -O2 -g1 -I$(LRC)/include
OWN_CFLAGS   := $(BASE_CFLAGS) -std=c89 -pedantic -Wall -Wextra -Werror \
                -Wdeclaration-after-statement -Wframe-larger-than=4096
LINK_CFLAGS  := $(BASE_CFLAGS) -std=c89 -pedantic -Wall -Wextra -Werror \
                -Wdeclaration-after-statement
LIBCO_CFLAGS := $(BASE_CFLAGS) -std=c89 -Wall -Wextra -Werror \
                -DLIBCO_MP -D_GNU_SOURCE -DHAVE_POSIX_MEMALIGN=1
LRC_CFLAGS   := $(BASE_CFLAGS) -std=gnu99 -Wall -pedantic -DHAVE_DYNAMIC

$(call obj,$(HARNESS_SOURCES)): FLAGS := $(OWN_CFLAGS) -DHAVE_DYNAMIC $(HARNESS_DEFINES)
$(call obj,$(CORE_SOURCES)):    FLAGS := $(OWN_CFLAGS) $(PIC)
$(call obj,$(LINK_SOURCES)):    FLAGS := $(LINK_CFLAGS)
$(call obj,$(LIBCO_SOURCES)):   FLAGS := $(LIBCO_CFLAGS)
$(call obj,$(LRC_SOURCES)):     FLAGS := $(LRC_CFLAGS)
$(call obj,$(HARNESS_SOURCES)): $(HARNESS_HEADERS)
$(call obj,$(CORE_SOURCES)):    ../core/link_test_render.h

ifneq ($(SANITIZER),)
SAN_FLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS   += -fsanitize=$(SANITIZER)
MIN_STACK := --min-stack 8388608
endif
RUNNER ?=

all: $(HARNESS) $(CORE)

.SECONDEXPANSION:
$(OBJ_DIR)/%.o: $$(subst __/,../,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(FLAGS) $(SAN_FLAGS) $(CFLAGS)

$(HARNESS): $(call obj,$(HARNESS_SOURCES) $(LINK_SOURCES) $(LIBCO_SOURCES) $(LRC_SOURCES))
	$(CC) -o $@ $^ $(LDFLAGS) $(DL_LIBS) $(GPU_LIBS)

$(CORE): $(call obj,$(CORE_SOURCES))
	$(CC) $(SHARED) -o $@ $^ $(LDFLAGS)

check: all
	@for scn in $(SCENARIOS); do \
	   $(call RUNS,$(BUILD_DIR)/out,--path test=$(CORE),$(ALL_RUNS)); \
	done
	@for pair in spans_waiting:340 spans_membership:300; do \
	   name=$${pair%%:*}; want=$${pair#*:}; trace="$(BUILD_DIR)/out/$$name/first/trace.txt"; \
	   n=`grep -c '^F [0-9]* C 0 N ' "$$trace"`; \
	   off=`awk '$$3 == "C" && $$4 == 0 && $$6 != $$2' "$$trace" | head -n 1`; \
	   [ "$$n" = "$$want" ] && [ -z "$$off" ] \
	      || { echo "check: $$name: console 0 finished $$n frames, not one a step: $$off"; exit 1; }; \
	done
	@grep -q '^c1 frame [0-9]* in_frame 1$$' "$(BUILD_DIR)/out/spans_waiting/first/parked/consoles.txt" \
	   || { echo "check: spans_waiting: console 1 is not parked at checkpoint parked"; exit 1; }
	@out="$(BUILD_DIR)/out/spans_membership/first"; \
	for pin in in_flight:c2:1 left:c2:1 back:c2:1 plugged:c1:1 pull:c1:0 pull:c2:1; do \
	   at=$${pin%%:*}; c=$${pin#*:}; in=$${c#*:}; c=$${c%%:*}; \
	   grep -q "^$$c frame [0-9]* in_frame $$in$$" "$$out/$$at/consoles.txt" \
	      || { echo "check: spans_membership: $$c is not in_frame $$in at checkpoint $$at"; exit 1; }; \
	done
	@$(call PARKED,spans_save_in_advance render_spans warm_save_in_advance)
	@n=`grep '^F [0-9]* C 0 N ' "$(BUILD_DIR)/out/render_one/first/trace.txt" \
	   | cut -d ' ' -f 8 | sort -u | wc -l`; \
	[ "$$n" -eq 120 ] || { echo "check: render_one: $$n different pictures in 120 frames"; exit 1; }
	@for pair in small:64 mid:128; do \
	   at=$${pair%%:*}; want=$${pair#*:}; png="$(BUILD_DIR)/out/render_grow/first/$$at/c0.png"; \
	   wide=`od -A n -t u1 -j 16 -N 4 "$$png" \
	      | awk '{ print (($$1 * 256 + $$2) * 256 + $$3) * 256 + $$4 }'`; \
	   [ "$$wide" = "$$want" ] \
	      || { echo "check: render_grow: $$at/c0.png is $$wide wide, not $$want"; exit 1; }; \
	done
	@for kept in render_two:1 render_dupe:0; do \
	   name=$${kept%%:*}; c=$${kept#*:}; out="$(BUILD_DIR)/out/$$name/first"; \
	   cmp -s "$$out/before/c$$c.png" "$$out/dupe/c$$c.png" \
	      || { echo "check: $$name: console $$c's picture at checkpoint dupe is not the one at before"; exit 1; }; \
	done
	@out="$(BUILD_DIR)/out/host_stamp"; \
	! cmp -s "$$out/first/trace.txt" "$$out/again/trace.txt" \
	   || { echo "check: host_stamp: the again run's trace is the first run's, S fields too"; exit 1; }; \
	edited="$(BUILD_DIR)/out/fail/stamp_edited.txt"; err="$(BUILD_DIR)/out/fail/stamp_edited.err"; \
	what="host_stamp --from --expect a trace with an S field edited"; \
	why="trace differs at frame 50 console 1:"; \
	mkdir -p "$(BUILD_DIR)/out/fail"; \
	sed 's/^\(F 50 C 1 .* S \)[0-9a-f]*$$/\1edited/' "$$out/first/trace.txt" > "$$edited"; \
	grep -q '^F 50 C 1 .* S edited$$' "$$edited" || { echo "check: no frame 50 line of console 1 to edit"; exit 1; }; \
	echo "== $$what, exit 1 wanted"; \
	$(RUNNER) $(HARNESS) $(MIN_STACK) --scenario ../scenarios/host_stamp.scn --path test=$(CORE) \
	   --from "$$out/first/mid.state" --expect "$$edited" \
	   --out "$(BUILD_DIR)/out/fail/stamp_edited" 2>"$$err"; \
	$(call FAILED_AS,1)
	@out="$(BUILD_DIR)/out/host_stamp"; \
	for f in V A; do \
	   edited="$(BUILD_DIR)/out/fail/stamp_fresh_$$f.txt"; err="$(BUILD_DIR)/out/fail/stamp_fresh_$$f.err"; \
	   what="host_stamp --expect a trace with field $$f edited"; \
	   why="trace differs at frame 20 console 1:"; \
	   sed "s/^\(F 20 C 1 .* $$f \)[0-9a-f]*/\1edited/" "$$out/first/trace.txt" > "$$edited"; \
	   grep -q "^F 20 C 1 .* $$f edited " "$$edited" \
	      || { echo "check: no frame 20 line of console 1 to edit"; exit 1; }; \
	   echo "== $$what, exit 1 wanted"; \
	   $(RUNNER) $(HARNESS) $(MIN_STACK) --scenario ../scenarios/host_stamp.scn --path test=$(CORE) \
	      --expect "$$edited" --out "$(BUILD_DIR)/out/fail/stamp_fresh_$$f" 2>"$$err"; \
	   $(call FAILED_AS,1); \
	done
	@trace="$(BUILD_DIR)/out/two_spans/first/trace.txt"; \
	n=`grep -c '^F [0-9]* C 1 N ' "$$trace"`; \
	last=`grep '^F [0-9]* C 1 N ' "$$trace" | tail -n 1 | cut -d ' ' -f 1-6`; \
	[ "$$n" = 598 ] && [ "$$last" = "F 599 C 1 N 597" ] \
	   || { echo "check: two_spans: console 1 finished $$n frames, the last $$last;" \
	      "598 wanted, the last F 599 C 1 N 597"; exit 1; }
	@$(call MUST_FAIL,$(BAD_SCENARIOS) $(FAIL_SCENARIOS))
	@head -c 100 "$(BUILD_DIR)/out/two_unequal/first/mid.state" > "$(BUILD_DIR)/out/bad.state" \
	   || { echo "check: two_unequal wrote no mid.state"; exit 1; }; \
	for run in truncated mismatched; do \
	   case $$run in \
	      truncated)  scn=two_unequal; state="$(BUILD_DIR)/out/bad.state"; \
	                  why="bad state: truncated" ;; \
	      mismatched) scn=ring_three;  state="$(BUILD_DIR)/out/two_unequal/first/mid.state"; \
	                  why="bad state: a state of another scenario" ;; \
	   esac; \
	   what="$$scn --from $$run state"; err="$(BUILD_DIR)/out/bad/$$run.err"; \
	   echo "== $$scn --from $$state, exit 2 wanted"; \
	   $(RUNNER) $(HARNESS) --scenario "../scenarios/$$scn.scn" --path test=$(CORE) \
	      --from "$$state" --out "$(BUILD_DIR)/out/bad/$$run" 2>"$$err"; \
	   $(call FAILED_AS,2); \
	done
	@edited="$(BUILD_DIR)/out/fail/edited.txt"; err="$(BUILD_DIR)/out/fail/edited.err"; \
	what="two_unequal --expect an edited trace"; why="trace differs at frame 60 console 1:"; \
	sed 's/^\(F 60 C 1 N [0-9]* V \)[0-9a-f]*/\1edited/' "$(BUILD_DIR)/out/two_unequal/first/trace.txt" \
	   > "$$edited"; \
	grep -q '^F 60 C 1 N [0-9]* V edited ' "$$edited" || { echo "check: no frame 60 line of console 1 to edit"; exit 1; }; \
	echo "== $$what, exit 1 wanted"; \
	$(RUNNER) $(HARNESS) --scenario ../scenarios/two_unequal.scn --path test=$(CORE) \
	   --expect "$$edited" --out "$(BUILD_DIR)/out/fail/edited" 2>"$$err"; \
	$(call FAILED_AS,1)
	$(call LOAD_FAILS,stale,first from,rewound,--rewind)
	$(call LOAD_FAILS,cold,first,from,--from)
ifeq ($(HAVE_EGL),1)
	$(call GPU_CHECK,gl,$(GPU_ARGS),gl/spans gl/mixed)
	$(GL_CHECK)
endif

check-gl: all
	$(call GPU_CHECK,gl,$(GPU_ARGS),gl/spans gl/mixed)
	$(GL_CHECK)

proof: $(HARNESS)
	@scn='$(PROOF)'; [ -n "$$scn" ] || { echo "proof: PROOF names no scenario"; exit 2; }; \
	$(call RUNS,$(BUILD_DIR)/proof,$(foreach b,$(BIND),--path '$(b)'),$(ALL_RUNS))

clean:
	build='$(BUILD_DIR)'; rm -rf -- "$${build:?}"

.PHONY: all check check-gl clean proof
