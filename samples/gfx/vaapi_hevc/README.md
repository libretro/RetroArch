VA HEVC ownership and Vulkan conversion checks
============================================

`make clean check SANITIZER=address,undefined` runs the portable parameter,
submission, surface-pool, PRIME export/import and lifetime fault checks.
Linux needs libva/libdrm development headers. The native binding probe
does not decode or export a hardware surface.

Opaque VA leases also test stale keys, abandonment, take/restore, GPU
retirement and producer-only native returns. Linux runs 10000 handoffs
through the existing SPSC queue. `make tsan-check CC=gcc` uses a separate
race-detection binary; set RUNNER to the host ASLR wrapper when needed.

`make gpu-check SANITIZER=address,undefined` additionally needs the Vulkan
loader, development library, validation layer and a device supporting
Vulkan 1.1, external-memory FD and foreign queue ownership. Mesa lavapipe
is sufficient. Missing capabilities fail this explicit test; they are
not reported as a successful or skipped hardware decode check.

The GPU oracle executes the production VA conversion submission helper
and existing YUV shaders on exportable OPAQUE_FD Vulkan plane images.
It checks cropped NV12/P010, full/limited range, BT.601/709 SDR and
BT.2020 PQ/HLG, RGBA8/linear FP16 output, three destination layouts,
repeated foreign ownership transfers, fences and validation errors.
Waits, staging uploads and pixel readback belong only to this oracle.

Ticket metadata is synthetic. No VA display, hardware decoder, DMA-BUF
FD import or DRM modifier image is created by the GPU oracle. Actual
VA decode/export/import pixels and native seek/shutdown acceptance
remain separate device gates.
