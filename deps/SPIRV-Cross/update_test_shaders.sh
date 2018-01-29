#!/bin/bash

echo "Building spirv-cross"
make -j$(nproc)

export PATH="./external/glslang-build/StandAlone:./external/spirv-tools-build/tools:.:$PATH"
echo "Using glslangValidation in: $(which glslangValidator)."
echo "Using spirv-opt in: $(which spirv-opt)."

./test_shaders.py shaders --update || exit 1
./test_shaders.py shaders --update --opt || exit 1
./test_shaders.py shaders-msl --msl --update || exit 1
./test_shaders.py shaders-msl --msl --update --opt || exit 1
./test_shaders.py shaders-msl-no-opt --msl --update || exit 1
./test_shaders.py shaders-hlsl --hlsl --update || exit 1
./test_shaders.py shaders-hlsl --hlsl --update --opt || exit 1

