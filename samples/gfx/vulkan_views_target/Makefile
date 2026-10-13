CC ?= cc
PYTHON ?= python3
BUILD_DIR ?= .build
REPO_ROOT := ../../..
TARGET := $(BUILD_DIR)/vulkan_views_target_test$(EXE)

CFLAGS += -Wall -Wextra -pedantic -std=c89 -Werror=declaration-after-statement \
          -Werror=implicit-function-declaration -Wno-long-long \
          -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
          -I$(REPO_ROOT)/gfx/include -I$(BUILD_DIR)

all: $(TARGET)

$(BUILD_DIR)/driver_code.inc: extract.py $(REPO_ROOT)/gfx/drivers/vulkan.c $(REPO_ROOT)/gfx/video_views_draw.h
	@mkdir -p $(BUILD_DIR)
	$(PYTHON) extract.py $@

$(TARGET): vulkan_views_target_test.c $(BUILD_DIR)/driver_code.inc
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

check: $(TARGET)
	$(RUNNER) $(TARGET)

clean:
	rm -f $(BUILD_DIR)/driver_code.inc $(TARGET)

.PHONY: all check clean
