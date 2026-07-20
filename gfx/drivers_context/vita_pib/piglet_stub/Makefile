all: out/liblibScePiglet.a

out/liblibScePiglet.a: out/Makefile
	$(MAKE) -C out

out/Makefile:
	vita-libs-gen nids.yml out

clean:
	$(RM) -r out

.PHONY: clean
