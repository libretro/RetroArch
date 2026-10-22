CC ?= cc
export CC SANITIZER
all check: session-check
session-check:
	python3 session_run.py
clean:
	rm -f test test.exe
.PHONY: all check session-check clean
