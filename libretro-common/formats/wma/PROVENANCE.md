# Source provenance

The C spectral decoder in `rwma_pro.c` adapts the MIT-licensed Go decoder
in [WaxFlow](https://github.com/ColeSpringer/WaxFlow), commit
`2f448c9435fd096961c3af55ba7e0a768644099d`, `codec/wmapro` and `codec/wma`.
Its copyright and license are retained in `COPYING.MIT.WaxFlow`.
The bit reader, C API, borrowed output interface, transform and tests are
original RetroArch code under `COPYING.MIT`.

`rwma_pro_data.h` restates WaxFlow's `tables_bands.go`, `tables_scale.go`,
`tables_coef.go` and `tables_decorr.go`. Prefix tries are constructed from
the listed code lengths and symbol ordering. Runs, levels, band edges and
decorrelation matrices retain their numeric values.

WaxFlow's `THIRD-PARTY-NOTICES.md` identifies these numeric format parameters
as data extracted from FFmpeg `libavcodec/wmaprodata.h`, n9.0, commit
`d32b387f2b0a484599d4587d651891f0c63c4238`. Its notice distinguishes this
parameter data from the independently implemented MIT decoder logic. That
origin is preserved here; these tables are not claimed to be an independent
measurement of Microsoft's encoder. No FFmpeg decoder implementation is
included in this replacement.

`rwma_standard_data.h` similarly restates `codec/wma/tables_coef.go`,
`tables_exp.go` and `tables_bands.go`: coefficient codewords, run/level
parameters, exponent codewords, LSP parameters and band partitions.
WaxFlow records these as numeric parameters from FFmpeg's `wmadata.h`,
`wma_freqs.c`, `aactab.c` and `wma.h`, under the same data-only policy.
Its WMA decoder logic is MIT licensed. The C adaptation computes envelope
lookups directly from the saved curve, without copying a resampled curve.
It returns the overlap accumulator by reference rather than copying a PCM
frame into a second output buffer.

The ASF field walk in `rwma_stream.c` adapts WaxFlow's MIT `container/asf`
reader at the same commit. The incremental borrowed payload cursor and
bounded reusable object assembly are original C code. RIFF/WAVE/XWMA object
layout follows Microsoft's DirectXTK Wave Formats documentation. Implicit
xWMA setup words (WMA2 `0x001f`, Pro `0x00e0` with 16-bit precision) were also
checked through the separate FFmpeg demuxer's reported configuration.

FFmpeg is used as a separate test oracle. Linking a test executable against
it does not make it a runtime dependency of rwma.
