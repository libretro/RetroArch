CC ?= cc
RA := ../../..
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
CORE := frontend_core$(if $(filter .exe,$(EXE)),.dll,.so)
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(CORE) rate_test$(EXE)
rate_fragment.c: $(RA)/record/drivers/record_nvenc.c
	sed -n '/^static bool record_nvenc_rate/,/^static bool record_nvenc_config/p' $< | sed '$$d' > $@
rate_test$(EXE): rate_test.c rate_fragment.c
	$(CC) $(CFLAGS) -o $@ rate_test.c $(LDFLAGS) -lm
$(CORE): core.c
	$(CC) $(CFLAGS) $(if $(filter .exe,$(EXE)),,-fPIC) -shared -o $@ $< $(LDFLAGS)
check: all
	$(RUNNER) ./rate_test$(EXE)
	python3 run.py --core ./$(CORE) $(if $(RETROARCH),--retroarch "$(RETROARCH)",)
clean:
	rm -f frontend_core.dll frontend_core.so rate_fragment.c rate_test rate_test.exe
.PHONY: all check clean
