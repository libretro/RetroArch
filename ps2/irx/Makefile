#Configuration for IRX
EE_BIN2C = bin2c
IRX_DIR = $(PS2SDK)/iop/irx

#IRX modules
# IRX modules - modules have to be in IRX_DIR
IRX_FILES += iomanX.irx fileXio.irx sio2man.irx mtapman.irx padman.irx mcman.irx mcserv.irx
IRX_FILES += usbd.irx bdm.irx bdmfs_vfat.irx usbmass_bd.irx
IRX_FILES += libsd.irx audsrv.irx cdfs.irx
IRX_C_FILES = $(IRX_FILES:.irx=_irx.c) 

all: irxs

# Specific file name and output per IRX Module
%.irx:
	$(EE_BIN2C) $(IRX_DIR)/$@ $(@:.irx=_irx.c) $(@:.irx=_irx)

irxs: $(IRX_FILES)

clean:
	rm -f $(IRX_C_FILES)

#Include preferences
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
