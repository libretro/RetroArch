CC ?= cc
export CC SANITIZER
all check: session-check frame-check export-check vulkan-check warm-check
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

vulkan-check:
	python3 vulkan_run.py
vulkan-gpu-check:
	python3 vulkan_run.py --gpu
	python3 vulkan_run.py --gpu --compile-shaders
.PHONY: vulkan-check vulkan-gpu-check

warm-check:
	python3 warm_run.py
.PHONY: warm-check

backend-check:
	python3 backend_run.py
all check: backend-check
.PHONY: backend-check
