CC ?= cc
export CC SANITIZER
all check: session-check frame-check export-check
session-check:
	python3 session_run.py
clean:
	rm -f test test.exe
.PHONY: all check session-check clean

frame-check:
	python3 frame_run.py
.PHONY: frame-check

export-check:
	python3 export_run.py
.PHONY: export-check
