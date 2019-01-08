
TARGET      = com-parse
DEBUG       = 0
GENDEPS     = 1
TARGET_ARCH = amd64
OS         ?= win32

OBJ :=
OBJ += com-parse.o

EXE_EXT := $(suffix $(wildcard $(MAKE).*))

ifeq ($(compiler),)
   ifeq ($(patsubst %.exe,%,$(basename $(CC))),cl)
      compiler = msvc
   else
      compiler = gcc
   endif
endif

CC_OUT  = -o $(NOTHING)
CXX_OUT = $(CC_OUT)
LD_OUT  = $(CC_OUT)
OBJ_EXT := o

ifeq ($(DEBUG),1)
   DEFINES += -DDEBUG -D_DEBUG
endif

ifeq ($(compiler),msvc)
   ARCH        = amd64
   ARCH2        = x64
   TARGET_ARCH2 = x64
   CROSS        = amd64
   WindowsSdkDir = C:\Program Files (x86)\Windows Kits\10\$(NOTHING)
   WindowsSDKVersion := 10.0.14393.0\$(NOTHING)
   VCINSTALLDIR := C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\$(NOTHING)

   INCLUDE := $(VCINSTALLDIR)include;$(VCINSTALLDIR)atlmfc\include;$(WindowsSdkDir)include\$(WindowsSDKVersion)ucrt;$(WindowsSdkDir)include\$(WindowsSDKVersion)shared;$(WindowsSdkDir)include\$(WindowsSDKVersion)um;
   LIB     := $(VCINSTALLDIR)LIB\$(CROSS);$(VCINSTALLDIR)atlmfc\lib\$(CROSS);$(WindowsSdkDir)lib\$(WindowsSDKVersion)ucrt\$(TARGET_ARCH2);$(WindowsSdkDir)lib\$(WindowsSDKVersion)um\$(TARGET_ARCH2);C:\Program Files (x86)\NVIDIA Corporation\Cg\lib.$(TARGET_ARCH2);C:\Program Files (x86)\Microsoft DirectX SDK (February 2010)\Lib\$(TARGET_ARCH2);
   LIBPATH := $(VCINSTALLDIR)LIB\$(CROSS);$(VCINSTALLDIR)atlmfc\lib\$(CROSS);

   PATH := $(shell IFS=$$'\n'; cygpath "$(VCINSTALLDIR)bin\\$(CROSS)"):$(shell IFS=$$'\n'; cygpath "$(WindowsSdkDir)\bin\\$(ARCH2)"):$(PATH)

   export INCLUDE := $(INCLUDE)
   export LIB     := $(LIB)
   export LIBPATH := $(LIBPATH)
   export PATH    := $(PATH)

   DEFINES :=
   FLAGS  += -nologo
   FLAGS  += -Gm- -Zc:inline -fp:precise -Zc:forScope -Gd -Oi -volatile:iso
   #FLAGS  += -GR-
   CFLAGS += -TC
   CXXFLAGS += -TP -EHsc
   WARNINGS += -WX -W3
   WARNINGS += -wd4101 -wd4996 -wd4244 -wd4267 -wd4090 -wd4305 -wd4146 -wd4334 -wd4018

   CC = cl.exe
   CXX = cl.exe
   LD = link.exe
   RC = rc.exe
   LIBS += shell32.lib user32.lib gdi32.lib comdlg32.lib winmm.lib ole32.lib
   LDFLAGS  += -nologo -wx -nxcompat -machine:$(TARGET_ARCH2)
   ifeq ($(DEBUG),1)
      FLAGS += -GS -Gy -Od -RTC1 -D_SECURE_SCL=1 -Zi
      FLAGS += -MDd
      LDFLAGS += -DEBUG
      DEFINES += -DDEBUG -D_DEBUG
   else
      FLAGS += -GS- -Gy- -O2 -Ob2 -GF -GT -Oy -Ot -D_SECURE_SCL=0
      FLAGS += -MD
   endif
   OBJ := $(OBJ:.o=.obj)
   LDFLAGS += -WX -SUBSYSTEM:WINDOWS -ENTRY:mainCRTStartup
   DEFINES := $(patsubst -f%,,$(DEFINES))
   LDFLAGS := $(patsubst -l%,%.lib,$(LDFLAGS))
   LIBS := $(filter-out -lm,$(LIBS))
   LIBS := $(patsubst -l%,%.lib,$(LIBS))
   DEPFLAGS = -showIncludes | tee $*.dtemp | sed /'Note: including file:'/d
   MAKEDEPS = echo $@: $< \\ > $*.depend && \
              grep 'Note: including file:' $*.dtemp \
              | sed '/$(subst \,\\,$(WindowsSdkDir))/Id; /$(subst \,\\,$(VCINSTALLDIR))/Id; s/Note: including file:[ ]*//g; s/\\/\//g; s/ /\\ /g; s/.*/   & \\/g' \
              >> $*.depend && \
              rm -f $*.dtemp

   OBJ_EXT := obj
   CC_OUT := -Fo:
   CXX_OUT := $(CC_OUT)
   LD_OUT := -out:
else
   RC := windres
   DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)$*.depend
   LD = $(CXX)
   ifeq ($(DEBUG),1)
      FLAGS += -g -O0
   else
      FLAGS += -O3
   endif
endif

INCLUDE_DIRS += -I. -I../../deps/peglib

$(info os       : $(OS))
$(info host     : $(ARCH))
$(info target   : $(TARGET_ARCH))
$(info compiler : $(compiler))

all: $(TARGET)$(EXE_EXT)

%.h:
	touch $*.tmp
	$(CXX) $*.tmp -DCINTERFACE -D_REFIID_DEFINED= -D_REFGUID_DEFINED= -D_HRESULT_DEFINED= \
   -EP -FI $@ $(FLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDE_DIRS) $(WARNINGS) > $@
	rm $*.tmp

SHELL:=$(SHELL) -o pipefail

ifeq ($(GENDEPS),0)
   DEPFLAGS :=
   MAKEDEPS :=
endif

%.$(OBJ_EXT): %.cpp
	$(CXX) -c $(CXX_OUT)$@ $< $(FLAGS) $(CXXFLAGS) $(DEFINES) $(INCLUDE_DIRS) $(WARNINGS) $(DEPFLAGS)
	@$(MAKEDEPS)

%.$(OBJ_EXT): %.c
	$(CC) -c $(CC_OUT)$@ $< $(FLAGS) $(CFLAGS) $(DEFINES) $(INCLUDE_DIRS) $(WARNINGS) $(DEPFLAGS)
	@$(MAKEDEPS)

%.res: %.rc
	$(RC) $<
	mv $*.res $@

$(TARGET)$(EXE_EXT): $(OBJ) .$(TARGET).last
	@touch .$(TARGET).last
	$(LD) $(OBJ) $(LDFLAGS) $(LIBS) $(LD_OUT)$@

%.depend: ;
%.last: ;
.FORCE:

clean:
	rm -f $(OBJ) $(TARGET)$(EXE_EXT)
	rm -f $(TARGET)
	rm -f .$(TARGET).last
	rm -f $(OBJ:.obj=.depend)

.PHONY: clean all
.PRECIOUS: %.depend %.last

-include $(patsubst %.obj,%.depend,$(filter %.obj,$(OBJ)))
