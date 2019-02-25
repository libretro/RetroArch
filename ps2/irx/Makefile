#Configuration for IRX
EE_BIN2C = bin2c
IRX_DIR = $(PS2SDK)/iop/irx

#IRX modules
# IRX modules - modules have to be in IRX_DIR
IRX_FILES += freemtap.irx freepad.irx freesio2.irx iomanX.irx fileXio.irx mcman.irx mcserv.irx usbd.irx usbhdfsd.irx 
IRX_FILES += freesd.irx audsrv.irx poweroff.irx
IRX_C_FILES = $(IRX_FILES:.irx=_irx.c)

# Specific file name and output per IRX Module
%.irx:
	$(EE_BIN2C) $(IRX_DIR)/$@ $(@:.irx=_irx.c) $(@:.irx=_irx)

all: $(IRX_FILES)

clean:
	rm -f $(IRX_C_FILES)

#Include preferences
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
