# Remove the line below, if you want to disable silent mode
#.SILENT:

all: lib/libcdvdfs.a lib/cdvd.irx

lib:
	mkdir -p $@

clean:
	$(MAKE) -C ee clean
	$(MAKE) -C iop clean

lib/cdvd.irx: iop | lib
	@echo Building IRX
	$(MAKE) -C $<

lib/libcdvdfs.a: ee | lib
	@echo Building EE client
	$(MAKE) -C $<
