all check:
	@test -n "$(OUTPUT_DIR)" || { echo "set OUTPUT_DIR to a scratch directory"; exit 1; }
	python3 build.py --output-dir "$(OUTPUT_DIR)" $(BUILD_ARGS)

.PHONY: all check
