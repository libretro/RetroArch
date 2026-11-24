REPO_ROOT := ../../..
D3D_SHADER_GENERATED_DIR := .generated
CC = x86_64-w64-mingw32-gcc
.DEFAULT_GOAL := check
.generated/d3d_builtin_data.h: FORCE
	cd $(REPO_ROOT) && python3 tools/d3d_shader_embed.py --check --output gfx/common/d3d_builtin_data.h
	cd $(REPO_ROOT) && python3 tools/d3d_shader_embed.py --output samples/gfx/d3d_builtin/$@ --test-output samples/gfx/d3d_builtin/.generated/d3d_builtin_cases.h
d3d_builtin_test.exe: d3d_builtin_test.c $(REPO_ROOT)/gfx/common/d3d_builtin.c .generated/d3d_builtin_data.h
	$(CC) -O2 -Wall -Wextra -Werror -Wno-unused-function -DD3D_BUILTIN_TEST -DHAVE_D3D9 -DHAVE_D3D10 -DHAVE_D3D11 -DHAVE_D3D12 -I$(REPO_ROOT)/libretro-common/include -I.generated d3d_builtin_test.c $(REPO_ROOT)/gfx/common/d3d_builtin.c -ld3d11 -luuid -ldxguid -o $@
d3d_builtin_production_test.exe: d3d_builtin_test.c $(REPO_ROOT)/gfx/common/d3d_builtin.c .generated/d3d_builtin_data.h
	$(CC) -O2 -Wall -Wextra -Werror -Wno-unused-function -DHAVE_D3D9 -DHAVE_D3D10 -DHAVE_D3D11 -DHAVE_D3D12 -I$(REPO_ROOT)/libretro-common/include -I.generated d3d_builtin_test.c $(REPO_ROOT)/gfx/common/d3d_builtin.c -ld3d11 -luuid -ldxguid -o $@
check: d3d_builtin_test.exe d3d_builtin_production_test.exe
	$(RUNNER) ./d3d_builtin_test.exe
	$(RUNNER) ./d3d_builtin_production_test.exe
clean:
	rm -f d3d_builtin_test.exe d3d_builtin_production_test.exe
	rm -rf .generated
.PHONY: check clean FORCE
FORCE:
