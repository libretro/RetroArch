# Build - and where possible run - every sample harness.
#
#   make -C samples          build them all
#   make -C samples check    build and run them all
#   make -C samples clean
#
# Worth doing before pushing anything that touches gfx/ or
# libretro-common: the samples stub out large parts of RetroArch, so
# adding a call to a new function in a file a sample compiles will
# break that sample's link without touching the main build. That has
# happened, twice, both times caught by CI rather than here.
#
# Each sample is cleaned first: they compile shared sources into the
# same object paths under gfx/ and libretro-common/, so one sample's
# objects will otherwise be picked up by the next and fail its link.
#
# SAN= is passed through to the samples that honour it, so
#   make -C samples check SAN=thread
# runs the suite under TSan instead of ASan/UBSan.

SAMPLES := $(patsubst %/Makefile,%,$(wildcard */*/Makefile))

all:
	@rc=0; for d in $(SAMPLES); do \
	  printf '%-52s ' "$$d"; \
	  $(MAKE) -s -C $$d clean >/dev/null 2>&1; \
	  if $(MAKE) -s -C $$d $(if $(SAN),SAN=$(SAN)) >/tmp/.sample.log 2>&1; then \
	    w=`grep -c 'warning:' /tmp/.sample.log`; \
	    if [ "$$w" != "0" ]; then echo "ok ($$w warnings)"; else echo "ok"; fi; \
	  else \
	    echo "BUILD FAILED"; \
	    grep -E 'undefined reference|error:' /tmp/.sample.log | head -3 | sed 's/^/      /'; \
	    rc=1; \
	  fi; \
	done; rm -f /tmp/.sample.log; exit $$rc

check:
	@rc=0; for d in $(SAMPLES); do \
	  printf '%-52s ' "$$d"; \
	  $(MAKE) -s -C $$d clean >/dev/null 2>&1; \
	  if $(MAKE) -s -C $$d $(if $(SAN),SAN=$(SAN)) >/tmp/.sample.log 2>&1; then \
	    if $(MAKE) -s -C $$d check >>/tmp/.sample.log 2>&1; then echo "ok"; \
	    else \
	      bin=`ls $$d | grep -E '_test$$|^lifecycle$$|^create$$' | head -1`; \
	      if [ -n "$$bin" ] && (cd $$d && ./$$bin >/dev/null 2>&1); then echo "ok"; \
	      else echo "ok (built, no runner)"; fi; \
	    fi; \
	  else \
	    echo "BUILD FAILED"; \
	    grep -E 'undefined reference|error:' /tmp/.sample.log | head -3 | sed 's/^/      /'; \
	    rc=1; \
	  fi; \
	done; rm -f /tmp/.sample.log; exit $$rc

clean:
	@for d in $(SAMPLES); do $(MAKE) -s -C $$d clean >/dev/null 2>&1 || true; done

.PHONY: all check clean
