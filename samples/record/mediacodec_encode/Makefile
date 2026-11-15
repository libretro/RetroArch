CC ?= cc
export CC SANITIZER
all: check
check:
	python3 run.py
receiver-check:
	python3 stream_receiver_check.py
worker-receiver-check:
	python3 stream_receiver_check.py --worker
enhanced-receiver-check:
	python3 stream_receiver_check.py --worker --codec hevc $(if $(MC_STREAM_ARTIFACT_DIR),--artifacts "$(MC_STREAM_ARTIFACT_DIR)/hevc")
	python3 stream_receiver_check.py --worker --codec hevc10 $(if $(MC_STREAM_ARTIFACT_DIR),--artifacts "$(MC_STREAM_ARTIFACT_DIR)/hevc10")
	python3 stream_receiver_check.py --worker --codec av1 $(if $(MC_STREAM_ARTIFACT_DIR),--artifacts "$(MC_STREAM_ARTIFACT_DIR)/av1")
clean:
	@:
.PHONY: all check receiver-check worker-receiver-check enhanced-receiver-check clean
