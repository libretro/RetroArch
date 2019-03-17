#Configuration for IRX
EE_BIN2C = bin2c
IRX_DIR = $(PS2SDK)/iop/irx

#Specific folder for cdvd.irx
LIBCDVD_DIR = ../libcdvd
LIBCDVD_IRX_DIR = $(LIBCDVD_DIR)/lib

#IRX modules
# IRX modules - modules have to be in IRX_DIR
IRX_FILES += freemtap.irx freepad.irx freesio2.irx iomanX.irx fileXio.irx mcman.irx mcserv.irx usbd.irx usbhdfsd.irx 
IRX_FILES += freesd.irx audsrv.irx poweroff.irx
IRX_C_FILES = $(IRX_FILES:.irx=_irx.c) cdvd_irx.c

all: cdvd irxs

# Specific file name and output per IRX Module
%.irx:
	$(EE_BIN2C) $(IRX_DIR)/$@ $(@:.irx=_irx.c) $(@:.irx=_irx)

irxs: $(IRX_FILES)

cdvd:
	$(MAKE) -C $(LIBCDVD_DIR)
	$(EE_BIN2C) $(LIBCDVD_IRX_DIR)/$@.irx $@_irx.c $@_irx

clean:
	rm -f $(IRX_C_FILES)
	$(MAKE) -C $(LIBCDVD_DIR) clean

#Include preferences
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
