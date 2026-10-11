# MIPS coroutine regression

Build from the repository root with a native MIPS compiler, or cross-compile
and run the executable on the matching target (for example with QEMU user mode):

```
cc -O2 -Iinclude libco/libco.c samples/libco/mips/mips_test.c -o mips-test
./mips-test
cc -O2 -fPIC -Iinclude libco/libco.c samples/libco/mips/mips_test.c -o mips-test-pic
./mips-test-pic
```

A zero exit status means that coroutine creation, primary-context identity,
repeated switches in both directions, floating-point register preservation,
PIC global access, and allocation overflow rejection passed. The test does
not require game content. Entry functions deliberately never return.

Run both builds for big- and little-endian o32, n32 and n64 targets, with
hard and soft float where supported by the toolchain and target libraries.
For o32 hard float, include both FP32 and FP64/FPXX modes where available.
The test uses the compiler's ABI macros to select the required FP registers.
PSP and PS2 select their dedicated backends and are outside this test's scope.
