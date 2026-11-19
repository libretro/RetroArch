#!/bin/sh
# Regenerates gfx/drivers/deko3d_shaders/menu_sm86.h: the Switch driver's
# menu shaders (deko3d_menu_vsh.glsl, deko3d_menu_fsh.glsl) for the
# Switch 2, compiled by Mesa's NAK for SM 8.6 - the compiler the Switch 2
# Vulkan port (OunceVK) runs on the console. And menu_effects_sm86.h: the
# menu effects from the Vulkan driver's GLSL, with pipeline_quad.vert
# for the effects drawn over the screen's quad.
#
# Needs a Mesa source tree with NAK (26.2 or later; OunceVK's is the one
# this was made with) in MESA, and meson, ninja, python3 with mako, a
# Rust toolchain with bindgen-cli, glslangValidator and a C compiler.
# Mesa is built once into MESA_BUILD (default $MESA/build-rarch), with
# tools/nak/nak_rarch_bridge.patch applied: the part of the generator
# that needs Mesa's headers. tools/nak/nak_shader_gen.c is the rest.
# Run from the tree root after changing the GLSL, and commit the header.
set -eu

MESA="${MESA:?set MESA to a Mesa source tree with NAK}"
BUILD="${MESA_BUILD:-$MESA/build-rarch}"
ROOT=$(pwd)
DIR=gfx/drivers/deko3d_shaders
OUT="$DIR/menu_sm86.h"
FX_OUT="$DIR/menu_effects_sm86.h"
VK=gfx/drivers/vulkan_shaders
TMP="${TMPDIR:-/tmp}/gen_nak_shaders.$$"
CC="${CC:-cc}"

mkdir -p "$TMP/bin"
trap 'rm -rf "$TMP"' EXIT

# The bridge, into Mesa once
if [ ! -f "$MESA/src/nouveau/compiler/nak_rarch_bridge.c" ]; then
   (cd "$MESA" && patch -p1 < "$ROOT/tools/nak/nak_rarch_bridge.patch")
fi

# NAK and what it needs, not the Vulkan driver: its precompiled kernels
# want tools this does not, so those are stand-ins; libelf only the
# driver needs
if [ ! -f "$BUILD/build.ninja" ]; then
   printf '#!/bin/sh\nexit 0\n' > "$TMP/bin/mesa_clc"
   printf '#!/bin/sh\nexit 0\n' > "$TMP/bin/vtn_bindgen2"
   chmod +x "$TMP/bin/mesa_clc" "$TMP/bin/vtn_bindgen2"
   sed -i "s/error('NVK requires libelf')/warning('NVK requires libelf')/" \
      "$MESA/meson.build"
   PATH="$TMP/bin:$PATH" meson setup "$BUILD" "$MESA" \
      -Dmesa-clc=system -Dvulkan-drivers=nouveau -Dgallium-drivers= \
      -Dplatforms= -Dglx=disabled -Degl=disabled -Dgbm=disabled \
      -Dllvm=disabled -Dbuildtype=release -Dzstd=disabled \
      -Dvalgrind=disabled -Dlibunwind=disabled
fi
LIBS="src/nouveau/compiler/libnak.a src/nouveau/compiler/libnak_rs.a
src/compiler/spirv/libvtn.a src/compiler/nir/libnir.a
src/compiler/libcompiler.a src/compiler/rust/libcompiler_c_helpers.a
src/compiler/rust/libcompiler_bindings.a src/util/libmesa_util.a
src/util/libmesa_util_simd.a src/util/libmesa_util_clflush.a
src/util/libmesa_util_clflushopt.a src/util/blake3/libblake3.a
src/c11/impl/libmesa_util_c11.a"
ninja -C "$BUILD" $LIBS

# The bridge with libnak's own flags, the generator as C89
FLAGS=$(cd "$BUILD" && python3 - <<'EOF'
import json, shlex
for c in json.load(open("compile_commands.json")):
    if "libnak.a.p" in c.get("output", c["command"]) and c["file"].endswith(".c"):
        args = shlex.split(c["command"])
        print(" ".join(shlex.quote(a) for a in args
                       if a.startswith(("-I", "-D")) or a.startswith("-std")))
        break
EOF
)
(cd "$BUILD" && eval "$CC $FLAGS -O1 -c \
   \"$MESA/src/nouveau/compiler/nak_rarch_bridge.c\" -o \"$TMP/bridge.o\"")
$CC -std=c89 -pedantic -Wall -O1 -c tools/nak/nak_shader_gen.c \
   -o "$TMP/gen.o"
set --
for l in $LIBS; do set -- "$@" "$BUILD/$l"; done
$CC -o "$TMP/nak_shader_gen" "$TMP/gen.o" "$TMP/bridge.o" \
   -Wl,--start-group "$@" -Wl,--end-group -lz -lpthread -lm -ldl -lstdc++

glslangValidator -V -S vert -o "$TMP/vsh.spv" "$DIR/deko3d_menu_vsh.glsl"
glslangValidator -V -S frag -o "$TMP/fsh.spv" "$DIR/deko3d_menu_fsh.glsl"
"$TMP/nak_shader_gen" "$OUT" \
   nxg_s2_menu_vsh vert "$TMP/vsh.spv" \
   nxg_s2_menu_fsh frag "$TMP/fsh.spv"

# The effects: Vulkan GLSL as glslang takes it
fx() {
   glslangValidator -V -S "$1" -o "$TMP/$2.spv" "$3"
}
fx vert fx_quad_vert          "$DIR/pipeline_quad.vert"
fx vert fx_ribbon_vert        "$VK/pipeline_ribbon.vert"
fx frag fx_ribbon_frag        "$VK/pipeline_ribbon.frag"
fx vert fx_ribbon_simple_vert "$VK/pipeline_ribbon_simple.vert"
fx frag fx_ribbon_simple_frag "$VK/pipeline_ribbon_simple.frag"
fx frag fx_snow_simple_frag   "$VK/pipeline_snow_simple.frag"
fx frag fx_snow_frag          "$VK/pipeline_snow.frag"
fx frag fx_bokeh_frag         "$VK/pipeline_bokeh.frag"
fx frag fx_snowflake_frag     "$VK/pipeline_snowflake.frag"
"$TMP/nak_shader_gen" "$FX_OUT" \
   nxg_s2_fx_quad_vert          vert "$TMP/fx_quad_vert.spv" \
   nxg_s2_fx_ribbon_vert        vert "$TMP/fx_ribbon_vert.spv" \
   nxg_s2_fx_ribbon_frag        frag "$TMP/fx_ribbon_frag.spv" \
   nxg_s2_fx_ribbon_simple_vert vert "$TMP/fx_ribbon_simple_vert.spv" \
   nxg_s2_fx_ribbon_simple_frag frag "$TMP/fx_ribbon_simple_frag.spv" \
   nxg_s2_fx_snow_simple_frag   frag "$TMP/fx_snow_simple_frag.spv" \
   nxg_s2_fx_snow_frag          frag "$TMP/fx_snow_frag.spv" \
   nxg_s2_fx_bokeh_frag         frag "$TMP/fx_bokeh_frag.spv" \
   nxg_s2_fx_snowflake_frag     frag "$TMP/fx_snowflake_frag.spv"

echo "wrote $OUT and $FX_OUT"
