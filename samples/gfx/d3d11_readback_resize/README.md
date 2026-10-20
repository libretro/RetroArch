Run `python3 run.py` to compile and exercise the shipping SDR D3D11 viewport
readback function with a small COM model. No GPU is required. The function and
window-clamping helper are extracted from production source, not duplicated.

A cached redraw changes the extent while a screenshot destination is sized
for its previous viewport. Growth, shrinkage and equal-area reshaping must
return false without changing any destination bytes or mapping staging data.
A subsequent stable screenshot succeeds. Unchanged screenshots preserve byte
conversion, and recording retains nonblocking map flags and busy handling.
The test compiles as strict C89 with warnings and the frame budget enforced.
Native Windows runs under GDB by default; CC and RUNNER may override tools.

`--source FILE` selects another complete d3d11.c for a negative control. The
previous implementation writes into the canary-protected destination and
fails at runtime. This portable model proves the guard behaviour; it does not
substitute for native device, window-transition or HDR readback acceptance.

The HDR admission-prefix probe additionally requires redraw to occur before
backbuffer acquisition. It rejects changed extents without acquiring a buffer,
accepts stable/idle captures, and rejects unavailable HDR. Use --kind hdr with
--source for an independent old-source lease-ordering negative control. This
probe covers admission ordering, not HDR pixel conversion or GPU capability.
