CC ?= cc
export CC SANITIZER
all: check
check:
	python3 run.py
receiver-check:
	python3 stream_receiver_check.py
clean:
	@:
.PHONY: all check receiver-check clean
