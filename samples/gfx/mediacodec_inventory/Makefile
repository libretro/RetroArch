CC ?= cc
export CC SANITIZER
all: check
check:
	python3 run.py
clean:
	rm -f test test.exe classes/.stamp
.PHONY: all check clean
