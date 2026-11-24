# Deterministic SNES NTSC setup

The SNES NTSC filter now builds its kernels with signed Q32.32 arithmetic.
Multiplication and fixed-point division round to nearest, ties away from zero;
integer divisions explicitly truncate toward zero. Transcendental functions use
bounded integer series and fixed constants. Public floating-point controls are
read by their IEEE bits, limited to the documented [-1, 1] range; non-finite
values become zero. Custom decoder coefficients are bounded to [-16, 16].
The normal generator calls no floating-point arithmetic or libm. The original
float generator remains available only with the test flag
`SNES_NTSC_REFERENCE_FLOAT`.

The kernel workspace remains heap-held. Packed kernel calculations use an
explicit unsigned 64-bit width before narrowing to the existing table format.
The scalar/SSE2/AVX2/NEON frame blitters and their buffer ownership are unchanged.
There is no new per-frame work or copy. A local x86-64 measurement of 50
initializations averaged 20.4 ms for fixed setup versus 2.5 ms for the historical
float setup; this cost occurs when creating/reconfiguring the filter.

Run `make check-fixed` here to check native and portable arithmetic against an
independent wide-integer oracle and compare 28 setups with the historical
non-fused float reference. It checks five presets, all ten controls at both
endpoints, custom decoder matrices and a nearly unit rolloff. Game-like and
noise fixtures cover every burst phase. The fixed image SHA256 is pinned and
must agree between native and portable arithmetic. `make check-fma` deliberately
enables FMA contraction and still requires the same recorded hashes. Ordinary
`make check` also covers hires, SIMD/scalar equivalence and worker splitting.
Python is needed for the comparison test, never to build the filter.

Measured RGB565 differences against the historical float reference on GCC 13:

| Setup | Pixels changed | Mean absolute error, 0â€“255 | PSNR |
| --- | ---: | ---: | ---: |
| Composite | 0.06% | 0.001275 | 68.624 dB |
| RF | 0.05% | 0.001133 | 69.123 dB |
| RGB | 0.49% | 0.010168 | 59.736 dB |
| S-video | 1.10% | 0.022647 | 56.237 dB |
| Monochrome | 0.81% | 0.033175 | 54.548 dB |

Every changed channel is at most one RGB565 quantization step. These intentional
rounding differences form the new baseline. The comparison test reports each
custom setup too and rejects greater differences. Linux x86-64, AArch64 under
QEMU and Windows x64 under Wine produce the same fixed image digest.
