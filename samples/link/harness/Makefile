# The link harness and the link test core. Everything built, and every
# run's output, goes under BUILD_DIR. check runs each scenario five times:
# first, again, in reversed pick order, in a shuffled one and from the
# first run's mid state, each later run's trace, messages.log and
# stuck.txt compared against the first run's. The from run must start at
# the scenario's save mid line and not run that line again; where the
# scenario restarts its ticks at a load, its messages.log is compared
# without them. An in_flight checkpoint, at the save frame, must list a
# message. Then each scenario in bad/ must exit 2 and each in fail/ exit
# 1, printing the error its "# error:" line names, as must a truncated
# state, a state of another scenario and an edited trace.

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
	$(LRC)/streams/interface_stream.c \
	$(LRC)/streams/memory_stream.c \
	$(LRC)/streams/trans_stream.c \
	$(LRC)/streams/trans_stream_pipe.c \
	$(LRC)/streams/trans_stream_deflate.c \
	$(LRC)/streams/file_stream.c \
	$(LRC)/string/rstrtod.c \
	$(LRC)/vfs/vfs_implementation.c \
	$(LRC)/time/rtime.c
CORE_SOURCES    := ../core/link_test_core.c
SCENARIOS       := $(sort $(wildcard ../scenarios/*.scn))
BAD_SCENARIOS   := $(sort $(wildcard ../scenarios/bad/*.scn))
FAIL_SCENARIOS  := $(sort $(wildcard ../scenarios/fail/*.scn))
UNTICK          := s/ stamp [0-9a-f]* landing [0-9a-f]*//

# After a run that must fail, named $$what: it exited $(1), and its stderr,
# in $$err, has $$why
FAILED_AS = rc=$$?; cat "$$err"; \
	[ $$rc -eq $(1) ] || { echo "check: $$what exited $$rc, not $(1)"; exit 1; }; \
	grep -F -q -e "$$why" "$$err" || { echo "check: $$what did not say: $$why"; exit 1; }

BASE_CFLAGS  := -O2 -g1 -I$(LRC)/include
OWN_CFLAGS   := $(BASE_CFLAGS) -std=c89 -pedantic -Wall -Wextra -Werror \
                -Wdeclaration-after-statement -Wframe-larger-than=4096
LINK_CFLAGS  := $(BASE_CFLAGS) -std=c89 -pedantic -Wall -Wextra -Werror \
                -Wdeclaration-after-statement
LIBCO_CFLAGS := $(BASE_CFLAGS) -std=c89 -Wall -Wextra -Werror \
                -DLIBCO_MP -D_GNU_SOURCE -DHAVE_POSIX_MEMALIGN=1
LRC_CFLAGS   := $(BASE_CFLAGS) -std=gnu99 -Wall -pedantic -DHAVE_DYNAMIC

$(call obj,$(HARNESS_SOURCES)): FLAGS := $(OWN_CFLAGS) -DHAVE_DYNAMIC
$(call obj,$(CORE_SOURCES)):    FLAGS := $(OWN_CFLAGS) $(PIC)
$(call obj,$(LINK_SOURCES)):    FLAGS := $(LINK_CFLAGS)
$(call obj,$(LIBCO_SOURCES)):   FLAGS := $(LIBCO_CFLAGS)
$(call obj,$(LRC_SOURCES)):     FLAGS := $(LRC_CFLAGS)
$(call obj,$(HARNESS_SOURCES)): link_scenario.h link_console.h

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
	$(CC) -o $@ $^ $(LDFLAGS) $(DL_LIBS)

$(CORE): $(call obj,$(CORE_SOURCES))
	$(CC) $(SHARED) -o $@ $^ $(LDFLAGS)

check: all
	@for scn in $(SCENARIOS); do \
	   name=`basename "$$scn" .scn`; out="$(BUILD_DIR)/out/$$name"; \
	   save=`sed -n 's/^\([0-9][0-9]*\) save mid\( .*\)*$$/\1/p' "$$scn"`; \
	   [ -n "$$save" ] || { echo "check: $$name has no save mid line"; exit 1; }; \
	   flight=`sed -n 's/^\([0-9][0-9]*\) checkpoint in_flight\( .*\)*$$/\1/p' "$$scn"`; \
	   [ -z "$$flight" ] || [ "$$flight" = "$$save" ] \
	      || { echo "check: $$name's in_flight checkpoint is not at its save frame"; exit 1; }; \
	   untick=; grep -q '^option [0-9][0-9]* link_test_restart_ticks on\( .*\)*$$' "$$scn" \
	      && untick='$(UNTICK)'; \
	   for run in first again reversed shuffled from; do \
	      case $$run in \
	         first)    args="--out $$out/first" ;; \
	         again)    args="--out $$out/again --expect $$out/first/trace.txt" ;; \
	         reversed) args="--pick reversed --out $$out/reversed --expect $$out/first/trace.txt" ;; \
	         shuffled) args="--pick shuffled:7 --out $$out/shuffled --expect $$out/first/trace.txt" ;; \
	         from)     args="--from $$out/first/mid.state --out $$out/from --expect $$out/first/trace.txt"; \
	                   rm -f -- "$$out/from/mid.state" ;; \
	      esac; \
	      rm -f -- "$$out/$$run/stuck.txt"; \
	      echo "== $$name $$run"; \
	      $(RUNNER) $(HARNESS) $(MIN_STACK) --scenario "$$scn" --path test=$(CORE) $$args \
	         || { echo "check: $$name $$run failed with exit $$?"; exit 1; }; \
	      case $$run in \
	         first) ;; \
	         from) start=`sed -n '1s/^F \([0-9]*\) .*/\1/p' "$$out/from/trace.txt"`; \
	            [ "$$start" = "$$save" ] || { echo "check: $$name from started at frame $$start, not $$save"; exit 1; }; \
	            [ ! -e "$$out/from/mid.state" ] || { echo "check: $$name from ran its save line again"; exit 1; }; \
	            want=`awk -v from="$$save" '$$2 + 0 >= from + 0' "$$out/first/messages.log" \
	               | sed "$$untick" | cksum`; \
	            got=`sed "$$untick" "$$out/from/messages.log" | cksum`; \
	            [ "$$want" = "$$got" ] ;; \
	         *) cmp -s "$$out/first/messages.log" "$$out/$$run/messages.log" ;; \
	      esac || { echo "check: $$name $$run: messages.log differs from the first run's"; exit 1; }; \
	      [ ! -e "$$out/first/stuck.txt" ] || cmp -s "$$out/first/stuck.txt" "$$out/$$run/stuck.txt" \
	         || { echo "check: $$name $$run: stuck.txt differs from the first run's"; exit 1; }; \
	      [ -z "$$flight" ] || grep -q '^msg ' "$$out/$$run/in_flight/bus.txt" \
	         || { echo "check: $$name $$run: no message in flight at the save"; exit 1; }; \
	   done; \
	done
	@mkdir -p "$(BUILD_DIR)/out/bad" "$(BUILD_DIR)/out/fail"; \
	for scn in $(BAD_SCENARIOS) $(FAIL_SCENARIOS); do \
	   name=`basename "$$scn" .scn`; kind=`dirname "$$scn"`; kind=`basename "$$kind"`; \
	   case $$kind in bad) want=2 ;; *) want=1 ;; esac; \
	   what="$$kind/$$name"; err="$(BUILD_DIR)/out/$$what.err"; \
	   why=`sed -n 's/^# error: //p' "$$scn"`; \
	   [ -n "$$why" ] || { echo "check: $$what has no # error: line"; exit 1; }; \
	   echo "== $$what, exit $$want wanted"; \
	   $(RUNNER) $(HARNESS) --scenario "$$scn" --path test=$(CORE) --out "$(BUILD_DIR)/out/$$what" \
	      2>"$$err"; \
	   $(call FAILED_AS,$$want); \
	done
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
	sed 's/^\(F 60 C 1 V \)[0-9a-f]*/\1edited/' "$(BUILD_DIR)/out/two_unequal/first/trace.txt" \
	   > "$$edited"; \
	grep -q '^F 60 C 1 V edited ' "$$edited" || { echo "check: no frame 60 line of console 1 to edit"; exit 1; }; \
	echo "== $$what, exit 1 wanted"; \
	$(RUNNER) $(HARNESS) --scenario ../scenarios/two_unequal.scn --path test=$(CORE) \
	   --expect "$$edited" --out "$(BUILD_DIR)/out/fail/edited" 2>"$$err"; \
	$(call FAILED_AS,1)

clean:
	build='$(BUILD_DIR)'; rm -rf -- "$${build:?}"

.PHONY: all check clean
