Vulkan SDR-core brightness and preset conformance
===============================================

Build the native Windows frontend harness using the same configuration and
object directory as RetroArch:

    MAKE_ARGS='OBJDIR=obj-unix/hdr-conformance C89_BUILD=' sh samples/gfx/hdr_metadata/build_brightness.sh
    python samples/gfx/hdr_metadata/run_brightness.py --output /fresh/results

The runner requires MinGW GCC, GDB and a Vulkan device supporting the requested
HDR output formats. It runs each assertion binary under GDB and checks actual
output metadata; unavailable output modes are failures, not successful coverage.
Use --harness to select a separately built harness.

The SDR libretro core deliberately leaves the unused XRGB8888 byte zero.
Stock rendering, one and two identity shader passes, and an FP16 SDR-owned
preset must preserve the same RGB. Every HDR frame is compared against an
independent gamma-2.4, Rec.709/Rec.2020 and ST.2084 reference at paper white
100 and 200 nits. SDR controls check the original source patches. The default
suite checks 21 cases with twelve captures each. Active preset pass counts
are asserted, so accidentally testing the stock pipeline cannot count as
preset acceptance.

A real translucent input overlay additionally checks that UI alpha blending
survives opaque game composition. Game RGB must not disappear when a preset
preserves an unused zero source alpha byte.

These are pre-compositor GPU output checks. They do not establish physical
monitor luminance, Windows HDR desktop state, or perceptual display accuracy.
No production shader transfer function, HDR uniform ABI or brightness default
is modified by this fixture. Output directories must not already exist.
