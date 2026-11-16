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
fractional-receiver-check:
	python3 stream_receiver_check.py --worker --source-rate 32040.5 $(if $(MC_STREAM_ARTIFACT_DIR),--artifacts "$(MC_STREAM_ARTIFACT_DIR)/avc")
	python3 stream_receiver_check.py --worker --source-rate 44100.5 --codec hevc10 $(if $(MC_STREAM_ARTIFACT_DIR),--artifacts "$(MC_STREAM_ARTIFACT_DIR)/hevc10")
	python3 stream_receiver_check.py --worker --source-rate 32040.5 --codec av1 $(if $(MC_STREAM_ARTIFACT_DIR),--artifacts "$(MC_STREAM_ARTIFACT_DIR)/av1")
clean:
	@:
.PHONY: all check receiver-check worker-receiver-check enhanced-receiver-check fractional-receiver-check clean
