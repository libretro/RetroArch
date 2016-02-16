#!/bin/sh
bison --defines=glslang_tab.cpp.h -o glslang_tab.cpp -t glslang/glslang/MachineIndependent/glslang.y
