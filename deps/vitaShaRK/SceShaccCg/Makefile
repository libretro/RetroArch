ifdef VITASDK
PREFIX = $(VITASDK)/bin/
endif

ARCH ?= $(PREFIX)arm-vita-eabi
AS = $(ARCH)-as
AR = $(ARCH)-ar
RANLIB = $(ARCH)-ranlib

TARGETS = libSceShaccCg_stub.a
TARGETS_WEAK = libSceShaccCg_stub_weak.a

SceShaccCg_OBJS = SceShaccCg_SceShaccCg_SceShaccCg_0205DE96.o SceShaccCg_SceShaccCg_SceShaccCg_07DDFC78.o SceShaccCg_SceShaccCg_SceShaccCg_0E1285A6.o SceShaccCg_SceShaccCg_SceShaccCg_152971B1.o SceShaccCg_SceShaccCg_SceShaccCg_17223BEB.o SceShaccCg_SceShaccCg_SceShaccCg_2654E73A.o SceShaccCg_SceShaccCg_SceShaccCg_268FAEE9.o SceShaccCg_SceShaccCg_sceShaccCgInitializeCompileOptions.o SceShaccCg_SceShaccCg_SceShaccCg_4595A388.o SceShaccCg_SceShaccCg_SceShaccCg_46FA0303.o SceShaccCg_SceShaccCg_SceShaccCg_56BFA825.o SceShaccCg_SceShaccCg_SceShaccCg_648739F3.o SceShaccCg_SceShaccCg_sceShaccCgCompileProgram.o SceShaccCg_SceShaccCg_SceShaccCg_6BB58825.o SceShaccCg_SceShaccCg_sceShaccCgSetDefaultAllocator.o SceShaccCg_SceShaccCg_SceShaccCg_6FB40CA9.o SceShaccCg_SceShaccCg_SceShaccCg_7B2CF324.o SceShaccCg_SceShaccCg_SceShaccCg_7BC25091.o SceShaccCg_SceShaccCg_SceShaccCg_7F430CCD.o SceShaccCg_SceShaccCg_SceShaccCg_95F57A23.o SceShaccCg_SceShaccCg_SceShaccCg_A067C481.o SceShaccCg_SceShaccCg_SceShaccCg_A13A8A1E.o SceShaccCg_SceShaccCg_SceShaccCg_A56B1A5B.o SceShaccCg_SceShaccCg_SceShaccCg_A7930FF6.o SceShaccCg_SceShaccCg_sceShaccCgInitializeCallbackList.o SceShaccCg_SceShaccCg_sceShaccCgDestroyCompileOutput.o SceShaccCg_SceShaccCg_SceShaccCg_B4AC9943.o SceShaccCg_SceShaccCg_SceShaccCg_BB703EE1.o SceShaccCg_SceShaccCg_SceShaccCg_D4378DB1.o SceShaccCg_SceShaccCg_SceShaccCg_DAD4AAE4.o SceShaccCg_SceShaccCg_SceShaccCg_DF3DDCFD.o SceShaccCg_SceShaccCg_SceShaccCg_EF8D59D6.o SceShaccCg_SceShaccCg_SceShaccCg_F4BAB902.o
SceShaccCg_weak_OBJS = SceShaccCg_SceShaccCg_SceShaccCg_0205DE96.wo SceShaccCg_SceShaccCg_SceShaccCg_07DDFC78.wo SceShaccCg_SceShaccCg_SceShaccCg_0E1285A6.wo SceShaccCg_SceShaccCg_SceShaccCg_152971B1.wo SceShaccCg_SceShaccCg_SceShaccCg_17223BEB.wo SceShaccCg_SceShaccCg_SceShaccCg_2654E73A.wo SceShaccCg_SceShaccCg_SceShaccCg_268FAEE9.wo SceShaccCg_SceShaccCg_sceShaccCgInitializeCompileOptions.wo SceShaccCg_SceShaccCg_SceShaccCg_4595A388.wo SceShaccCg_SceShaccCg_SceShaccCg_46FA0303.wo SceShaccCg_SceShaccCg_SceShaccCg_56BFA825.wo SceShaccCg_SceShaccCg_SceShaccCg_648739F3.wo SceShaccCg_SceShaccCg_sceShaccCgCompileProgram.wo SceShaccCg_SceShaccCg_SceShaccCg_6BB58825.wo SceShaccCg_SceShaccCg_sceShaccCgSetDefaultAllocator.wo SceShaccCg_SceShaccCg_SceShaccCg_6FB40CA9.wo SceShaccCg_SceShaccCg_SceShaccCg_7B2CF324.wo SceShaccCg_SceShaccCg_SceShaccCg_7BC25091.wo SceShaccCg_SceShaccCg_SceShaccCg_7F430CCD.wo SceShaccCg_SceShaccCg_SceShaccCg_95F57A23.wo SceShaccCg_SceShaccCg_SceShaccCg_A067C481.wo SceShaccCg_SceShaccCg_SceShaccCg_A13A8A1E.wo SceShaccCg_SceShaccCg_SceShaccCg_A56B1A5B.wo SceShaccCg_SceShaccCg_SceShaccCg_A7930FF6.wo SceShaccCg_SceShaccCg_sceShaccCgInitializeCallbackList.wo SceShaccCg_SceShaccCg_sceShaccCgDestroyCompileOutput.wo SceShaccCg_SceShaccCg_SceShaccCg_B4AC9943.wo SceShaccCg_SceShaccCg_SceShaccCg_BB703EE1.wo SceShaccCg_SceShaccCg_SceShaccCg_D4378DB1.wo SceShaccCg_SceShaccCg_SceShaccCg_DAD4AAE4.wo SceShaccCg_SceShaccCg_SceShaccCg_DF3DDCFD.wo SceShaccCg_SceShaccCg_SceShaccCg_EF8D59D6.wo SceShaccCg_SceShaccCg_SceShaccCg_F4BAB902.wo
SceKernel_OBJS =
ALL_OBJS=

all: $(TARGETS) $(TARGETS_WEAK)

define LIBRARY_template
 $(1): $$($(1:lib%_stub.a=%)_OBJS)
 ALL_OBJS += $$($(1:lib%_stub.a=%)_OBJS)
endef
define LIBRARY_WEAK_template
 $(1): $$($(1:lib%_stub_weak.a=%)_weak_OBJS)
 ALL_OBJS += $$($(1:lib%_stub_weak.a=%)_weak_OBJS)
endef

$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))
$(foreach library,$(TARGETS_WEAK),$(eval $(call LIBRARY_WEAK_template,$(library))))

install: $(TARGETS) $(TARGETS_WEAK)
	cp $(TARGETS) $(VITASDK)/arm-vita-eabi/lib
	cp $(TARGETS_WEAK) $(VITASDK)/arm-vita-eabi/lib

clean:
	rm -f $(TARGETS) $(TARGETS_WEAK) $(ALL_OBJS)

$(TARGETS) $(TARGETS_WEAK):
	$(AR) cru $@ $?
	$(RANLIB) $@

%.o: %.S
	$(AS) --defsym GEN_WEAK_EXPORTS=0 $< -o $@

%.wo: %.S
	$(AS) --defsym GEN_WEAK_EXPORTS=1 $< -o $@
