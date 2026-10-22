# rslc corpus census

Dump: libretro/slang-shaders at afcf996, through rslc_corpus_dump on 2026-10-22. 1631 shaders compile with the vendored glslang (3262 stages); 22 do not and are excluded.

Every row is a construct, the number of stages that use it, and up to three example stages. A construct with one stage is still in the dialect: that shader works today.

## version

| construct | stages | examples |
|---|---:|---|
| `450` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |

## extension

| construct | stages | examples |
|---|---:|---|
| `GL_GOOGLE_cpp_style_line_directive:require` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `GL_ARB_gpu_shader_int64:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_EXT_control_flow_attributes:enable` | 2 | edge-smoothing_vectorscale_shaders_optimize-energy:frag, edge-smoothing_vectorscale_shaders_optimize-energy:vert |
| `GL_EXT_shader_16bit_storage:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_EXT_shader_explicit_arithmetic_types:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_GOOGLE_include_directive:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_KHR_shader_subgroup_arithmetic:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_KHR_shader_subgroup_ballot:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_KHR_shader_subgroup_quad:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_KHR_shader_subgroup_shuffle:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `GL_NV_shader_atomic_int64:require` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |

## cpp

| construct | stages | examples |
|---|---:|---|
| `define` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `extension` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `line "file"` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `version` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `pragma parameter` | 2147 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `define function-like` | 1142 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `endif` | 1010 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_smaa_smaa-pass0:frag |
| `ifdef` | 724 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `ifndef` | 621 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_smaa_smaa-pass0:frag |
| `else` | 485 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `define multi-line` | 282 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt:frag, bezel_uborder_shaders_content_bezel_shaders_crt-nobody-bezel-reflections:frag |
| `undef` | 162 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag |
| `if` | 103 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_smaa_smaa-pass0:frag |
| `if arithmetic` | 57 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_smaa_smaa-pass0:frag |
| `elif` | 45 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `if defined()` | 33 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `pragma alias` | 32 | bezel_Mega_Bezel_shaders_base_text-adv-glass:frag, bezel_Mega_Bezel_shaders_base_text-adv-glass:vert, bezel_Mega_Bezel_shaders_base_text-adv-no-reflect:frag |
| `error` | 22 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `elif defined()` | 19 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `elif arithmetic` | 9 | bezel_uborder_shaders_content_bezel_shaders_crt_crt-pi-bezel-reflections:frag, bezel_uborder_shaders_content_koko_ambi_crt_crt-pi:frag, crt_shaders_crt-pi:frag |
| `layout` | 4 | procedural_fizzer-power-coils:frag, procedural_fizzer-power-coils:vert, procedural_fizzer-the-popular-shader:frag |

## type

| construct | stages | examples |
|---|---:|---|
| `mat4` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `vec4` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `void` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `vec2` | 3261 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `uint` | 2775 | annotated_passthru:frag, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `float` | 2612 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `vec3` | 1951 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, annotated_passthru:vert |
| `sampler2D` | 1734 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `int` | 1029 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `bool` | 828 | annotated_passthru:frag, annotated_passthru:vert, anti-aliasing_shaders_fxaa:frag |
| `ivec2` | 507 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `mat2x2` | 390 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `mat4x3` | 373 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `mat3x3` | 332 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:vert, bezel_Mega_Bezel_shaders_base_linearize-crt:frag |
| `mat3` | 330 | bezel_Mega_Bezel_shaders_base_combine-passes-hdr:frag, bezel_Mega_Bezel_shaders_base_combine-passes-hdr:vert, bezel_Mega_Bezel_shaders_base_combine-passes-no-reflect-hdr:frag |
| `bvec3` | 327 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `bvec4` | 322 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `bvec2` | 316 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_smaa_smaa-pass0:frag |
| `mat4x4` | 311 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:vert, bezel_Mega_Bezel_shaders_base_linearize-crt:frag |
| `mat2x4` | 234 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:vert, bezel_Mega_Bezel_shaders_base_linearize-crt:frag |
| `uvec2` | 130 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `ivec4` | 108 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `ivec3` | 106 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `mat3x4` | 53 | edge-smoothing_cunny_shaders_CuNNy-3x12-nvl-conv1:frag, edge-smoothing_cunny_shaders_CuNNy-3x12-nvl-conv2:frag, edge-smoothing_cunny_shaders_CuNNy-3x12-nvl-conv3:frag |
| `mat2` | 42 | crt_crt-effects_shaders_analog-service-menu:frag, edge-smoothing_cleanEdge_shaders_cleanEdge-scale:frag, edge-smoothing_cleanEdge_shaders_cleanEdge-scale:vert |
| `uvec4` | 42 | crt_shaders_cathode-retro_cathode-retro-crt-generate-masks:frag, crt_shaders_cathode-retro_cathode-retro-crt-generate-masks:vert, crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag |
| `uvec3` | 39 | crt_shaders_cathode-retro_cathode-retro-crt-generate-masks:frag, crt_shaders_cathode-retro_cathode-retro-crt-generate-masks:vert, crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag |
| `mat2x3` | 24 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_uborder_shaders_content_shaders_crt-nobody:frag |
| `sampler` | 20 | crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt_no-signal:frag |
| `mat4x2` | 13 | edge-smoothing_nedi_shaders_nedi-hybrid-pass0:frag, edge-smoothing_nedi_shaders_nedi-hybrid-pass0:vert, edge-smoothing_nedi_shaders_nedi-hybrid-pass1:frag |
| `mat3x2` | 11 | crt_shaders_mame_hlsl_shaders_mame_chroma:frag, crt_shaders_mame_hlsl_shaders_old_shaders_mame_hlsl_shaders_mame_chroma:frag, interpolation_shaders_lanczos2-5-taps:frag |
| `texture2D` | 4 | crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt_no-signal:frag |
| `double` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `dvec2` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `dvec3` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `dvec4` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `f16vec2` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `f16vec3` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `f16vec4` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `float16_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `i64vec2` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `int16_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `int64_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `int8_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `u64vec2` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `uint16_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `uint64_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `uint8_t` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |

## qualifier

| construct | stages | examples |
|---|---:|---|
| `std140` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `uniform` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `in` | 3259 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `out` | 3259 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `push_constant` | 2866 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `const` | 1436 | anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_aa-shader-4.0:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `inout` | 181 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `flat` | 67 | bezel_koko-aio_shaders-ng_ambi_temporal_pass:frag, bezel_koko-aio_shaders-ng_ambi_temporal_pass:vert, bezel_koko-aio_shaders-ng_avglum_pass:frag |
| `noperspective` | 34 | bezel_koko-aio_shaders-ng_ambi_temporal_pass:frag, bezel_koko-aio_shaders-ng_ambi_temporal_pass:vert, bezel_koko-aio_shaders-ng_avglum_pass:frag |
| `mediump` | 13 | crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt_no-signal:frag |
| `highp` | 9 | blurs_shaders_gizmo-blur:frag, crt_shaders_gizmo-crt:frag, crt_shaders_gizmo-slotmask-crt:frag |
| `sample` | 6 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `lowp` | 1 | handheld_shaders_gbc_pokemon_modernizer:frag |

## layout

| construct | stages | examples |
|---|---:|---|
| `binding` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `std140` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `location` | 3259 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `set` | 3259 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `push_constant` | 2866 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |

## block

| construct | stages | examples |
|---|---:|---|
| `uniform` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `push_constant` | 2866 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `more-than-two` | 4 | crt_shaders_crt-royale_src_crt-royale-geometry-aa-last-pass-intel:frag, crt_shaders_crt-royale_src_crt-royale-geometry-aa-last-pass-intel:vert, crt_shaders_crt-royale_src_crt-royale-geometry-aa-last-pass:frag |

## struct

| construct | stages | examples |
|---|---:|---|
| `definition` | 103 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `varying` | 42 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |

## array

| construct | stages | examples |
|---|---:|---|
| `declaration` | 393 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `dynamic-index` | 324 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `constructor` | 186 | bezel_Mega_Bezel_shaders_guest_hsm-deconvergence-no-reflect:frag, bezel_Mega_Bezel_shaders_guest_hsm-deconvergence-ntsc_orig:frag, bezel_Mega_Bezel_shaders_guest_hsm-deconvergence-potato:frag |
| `multidimensional` | 56 | bezel_Mega_Bezel_shaders_hyllian_crt-super-xbr_crt-hyllian-sinc-curvature:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag |
| `const-array` | 6 | crt_shaders_crt-beans_scanlines_analytical:frag, crt_shaders_crt-beans_scanlines_analytical:vert, crt_shaders_crt-beans_scanlines_cubic:frag |

## precision

| construct | stages | examples |
|---|---:|---|
| `on-declaration` | 19 | blurs_shaders_gizmo-blur:frag, crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag |
| `default-statement` | 15 | crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt_no-signal:frag |

## keyword

| construct | stages | examples |
|---|---:|---|
| `return` | 1697 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_aa-shader-4.0:vert |
| `if` | 1390 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag |
| `else` | 1163 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert |
| `for` | 460 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `break` | 155 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, bezel_Mega_Bezel_shaders_fxaa_fxaa:frag |
| `struct` | 103 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `while` | 63 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `case` | 53 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `switch` | 53 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `do` | 51 | bezel_Mega_Bezel_shaders_guest_extras_hsm-drez-g-sharp_resampler:frag, bezel_Mega_Bezel_shaders_guest_extras_hsm-g-sharp_resampler:frag, bezel_Mega_Bezel_shaders_guest_extras_hsm-screen-scale-g-sharp_resampler-no-reflect:frag |
| `default` | 46 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `discard` | 18 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `precision` | 15 | crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt_no-signal:frag |
| `continue` | 14 | edge-smoothing_vectorscale_shaders_cell-rasterizer-multi-aa:frag, edge-smoothing_vectorscale_shaders_cell-rasterizer-single-aa:frag, edge-smoothing_vectorscale_shaders_gradient-correction:frag |

## control

| construct | stages | examples |
|---|---:|---|
| `for` | 460 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `while` | 63 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `switch` | 53 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `do-while` | 51 | bezel_Mega_Bezel_shaders_guest_extras_hsm-drez-g-sharp_resampler:frag, bezel_Mega_Bezel_shaders_guest_extras_hsm-g-sharp_resampler:frag, bezel_Mega_Bezel_shaders_guest_extras_hsm-screen-scale-g-sharp_resampler-no-reflect:frag |

## decl

| construct | stages | examples |
|---|---:|---|
| `multiple-declarators` | 663 | annotated_passthru:frag, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0:frag |
| `for-init-declaration` | 432 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `multiple-declarators-initialised` | 103 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_mdapt_hsm-mdapt-pass0:frag |

## const

| construct | stages | examples |
|---|---:|---|
| `global-or-local` | 1356 | anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_aa-shader-4.0:vert, anti-aliasing_shaders_ewa_curvature:frag |

## expr

| construct | stages | examples |
|---|---:|---|
| `swizzle` | 2462 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, annotated_passthru:vert |
| `swizzle lvalue` | 1024 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_advanced-aa:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `ternary` | 997 | annotated_passthru:vert, anti-aliasing_shaders_advanced-aa:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `swizzle repeated-component` | 638 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `matrix-element` | 45 | bezel_Mega_Bezel_shaders_hyllian_crt-super-xbr_crt-hyllian-sinc-curvature:frag, bezel_scanline-classic_shaders_color-hdr:vert, bezel_scanline-classic_shaders_color-sdr:vert |

## op

| construct | stages | examples |
|---|---:|---|
| `:` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `+=` | 1056 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `==` | 1008 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_advanced-aa:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `?` | 997 | annotated_passthru:vert, anti-aliasing_shaders_advanced-aa:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `*=` | 818 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_ewa_curvature:frag |
| `&` | 744 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `&&` | 718 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `\|` | 657 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `\|\|` | 652 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `!` | 603 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `-=` | 536 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `<=` | 507 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `>=` | 477 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `!=` | 395 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `++` | 389 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert |
| `/=` | 374 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `%` | 146 | bezel_koko-aio_shaders-ng_ambi_temporal_pass:frag, bezel_koko-aio_shaders-ng_ambi_temporal_pass:vert, bezel_koko-aio_shaders-ng_avglum_pass:frag |
| `>>` | 145 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `^` | 95 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `^=` | 86 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `--` | 56 | bezel_Mega_Bezel_shaders_base_text-adv-glass:frag, bezel_Mega_Bezel_shaders_base_text-adv-no-reflect:frag, bezel_Mega_Bezel_shaders_base_text-adv:frag |
| `<<` | 30 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `\|=` | 9 | edge-smoothing_hqx_shaders_hq2x-halphon:frag, edge-smoothing_hqx_shaders_hq2x-halphon:vert, edge-smoothing_omniscale_shaders_omniscale:frag |
| `~` | 4 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag, edge-smoothing_vectorscale_shaders_resolve-crossings:frag |
| `>>=` | 2 | edge-smoothing_omniscale_shaders_omniscale:frag, edge-smoothing_omniscale_shaders_omniscale:vert |
| `&=` | 1 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag |

## literal

| construct | stages | examples |
|---|---:|---|
| `exponent` | 625 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `uint` | 125 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `hex` | 115 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |

## constructor

| construct | stages | examples |
|---|---:|---|
| `vec4` | 1757 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `vec2` | 1714 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_advanced-aa:vert |
| `vec3` | 1406 | annotated_passthru:frag, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `scalar-cast` | 1119 | annotated_passthru:frag, annotated_passthru:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `float` | 792 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_smaa_smaa-pass0:frag |
| `int` | 452 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `ivec2` | 420 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `matrix` | 395 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `mat3` | 317 | bezel_Mega_Bezel_shaders_base_combine-passes-hdr:frag, bezel_Mega_Bezel_shaders_base_combine-passes-hdr:vert, bezel_Mega_Bezel_shaders_base_combine-passes-no-reflect-hdr:frag |
| `vector-from-mixed` | 199 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `bool` | 188 | annotated_passthru:frag, annotated_passthru:vert, bezel_Mega_Bezel_shaders_mdapt_hsm-mdapt-pass0:frag |
| `mat4` | 140 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_Mega_Bezel_shaders_hyllian_crt-super-xbr_custom-bicubic-x:frag |
| `mat4x3` | 128 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `uvec2` | 93 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `uint` | 73 | bezel_Mega_Bezel_shaders_base_combine-passes-hdr:frag, bezel_Mega_Bezel_shaders_base_combine-passes-hdr:vert, bezel_Mega_Bezel_shaders_base_combine-passes-no-reflect-hdr:frag |
| `mat3x4` | 53 | edge-smoothing_cunny_shaders_CuNNy-3x12-nvl-conv1:frag, edge-smoothing_cunny_shaders_CuNNy-3x12-nvl-conv2:frag, edge-smoothing_cunny_shaders_CuNNy-3x12-nvl-conv3:frag |
| `mat3x3` | 45 | bezel_Mega_Bezel_shaders_gtu_hsm-gtu-pass1:frag, bezel_Mega_Bezel_shaders_gtu_hsm-gtu-pass1:vert, bezel_Mega_Bezel_shaders_gtu_hsm-gtu-pass2:frag |
| `mat2` | 40 | crt_crt-effects_shaders_analog-service-menu:frag, edge-smoothing_cleanEdge_shaders_cleanEdge-scale:frag, edge-smoothing_cleanEdge_shaders_cleanEdge-scale:vert |
| `mat4x4` | 20 | bezel_Mega_Bezel_shaders_easymode_hsm-crt-easymode-halation:frag, bezel_Mega_Bezel_shaders_easymode_hsm-crt-easymode-halation:vert, bezel_uborder_shaders_content_bezel_shaders_crt_crt-aperture-bezel-reflections:frag |
| `bvec3` | 18 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_color-hdr:frag |
| `matrix-from-components` | 18 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-source-pass:frag |
| `bvec4` | 13 | bezel_Mega_Bezel_shaders_scalefx_hsm-scalefx-pass3:frag, edge-smoothing_sabr_shaders_sabr-hybrid-deposterize:frag, edge-smoothing_sabr_shaders_sabr-hybrid-deposterize:vert |
| `ivec4` | 12 | bezel_koko-aio_shaders-ng_fxaa:frag, crt_shaders_crtsim_composite:frag, edge-smoothing_xbrz_shaders_2xbrz:frag |
| `mat2x3` | 12 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_uborder_shaders_content_shaders_crt-nobody:frag |
| `ivec3` | 9 | bezel_scanline-classic_shaders_famicom-rgb:frag, crt_shaders_crtsim_frame:frag, crt_shaders_crtsim_frame:vert |
| `mat3x2` | 8 | ntsc_shaders_patchy-ntsc_patchy-color:frag, ntsc_shaders_patchy-ntsc_patchy-color:vert, ntsc_shaders_patchy-ntsc_patchy-ntsc-combine-y-c:frag |
| `mat4x2` | 7 | edge-smoothing_nedi_shaders_nedi-hybrid-pass0:frag, edge-smoothing_nedi_shaders_nedi-hybrid-pass1:frag, edge-smoothing_nedi_shaders_nedi-hybrid-pass2:frag |
| `mat2x2` | 4 | crt_crt-effects_shaders_royale-curve-append:frag, crt_shaders_crt-royale_src_crt-royale-geometry-aa-last-pass-intel:frag, crt_shaders_crt-royale_src_crt-royale-geometry-aa-last-pass:frag |
| `uvec4` | 4 | crt_shaders_simple-crt_simple-crt:frag, crt_shaders_simple-crt_simple-crt:vert, edge-smoothing_vectorscale_shaders_cell-graph:frag |
| `bvec2` | 3 | bezel_Mega_Bezel_shaders_scalefx_hsm-scalefx-pass3:frag, edge-smoothing_scalefx_shaders_old_scalefx-pass2:frag, edge-smoothing_scalefx_shaders_scalefx-pass3:frag |
| `uvec3` | 1 | misc_shaders_ss-gamma-ramp:frag |

## builtin-var

| construct | stages | examples |
|---|---:|---|
| `gl_Position` | 1628 | anamorphic_shaders_anamorphic:vert, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:vert |
| `gl_FragCoord` | 199 | bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-no-reflect:frag, bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-no-reflect:vert, bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-ntsc-pass1:frag |

## builtin-fn

| construct | stages | examples |
|---|---:|---|
| `texture` | 1692 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `clamp` | 1344 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `mix` | 1281 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `max` | 1167 | annotated_passthru:vert, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag |
| `abs` | 1100 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_aa-shader-4.0:frag |
| `dot` | 1081 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_aa-shader-4.0:frag |
| `floor` | 1074 | anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag, auto-box_box-max:vert |
| `fract` | 1048 | anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa:frag |
| `pow` | 1033 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `min` | 1014 | annotated_passthru:vert, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag |
| `mod` | 862 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert |
| `sin` | 735 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert |
| `sqrt` | 627 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert |
| `exp` | 439 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-vert:frag |
| `cos` | 427 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_reflection-glass-hdr:frag |
| `sign` | 386 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `length` | 381 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `smoothstep` | 373 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `ceil` | 342 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert |
| `textureLod` | 327 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `step` | 318 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `atan` | 306 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag |
| `textureSize` | 286 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert |
| `inversesqrt` | 285 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:vert, bezel_Mega_Bezel_shaders_base_linearize-crt:frag |
| `round` | 251 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `dFdx` | 231 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:vert |
| `dFdy` | 231 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:vert |
| `log2` | 188 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:vert, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `log` | 160 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_grade_orig:vert, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag |
| `texelFetch` | 160 | crt_shaders_crt-beans_blur_horizontal:frag, crt_shaders_crt-beans_blur_vertical:frag, crt_shaders_crt-beans_calculate_widths:frag |
| `any` | 152 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt:frag, bezel_koko-aio_shaders-ng_fxaa:frag |
| `cross` | 141 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `normalize` | 139 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_reflection-glass-hdr:frag |
| `textureLodOffset` | 135 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `not` | 132 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt:frag, blurs_shaders_royale_blur10x10shared-gamma-encode-every-fbo:frag |
| `tanh` | 130 | bezel_Mega_Bezel_shaders_base_linearize-crt-hdr:frag, bezel_Mega_Bezel_shaders_base_linearize-crt:frag, bezel_scanline-classic_shaders_limiter:frag |
| `lessThan` | 122 | bezel_Mega_Bezel_shaders_base_text-adv-glass:frag, bezel_Mega_Bezel_shaders_base_text-adv-no-reflect:frag, bezel_Mega_Bezel_shaders_base_text-adv:frag |
| `lessThanEqual` | 111 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `all` | 102 | bezel_Mega_Bezel_shaders_base_text-adv-glass:frag, bezel_Mega_Bezel_shaders_base_text-adv-no-reflect:frag, bezel_Mega_Bezel_shaders_base_text-adv:frag |
| `greaterThan` | 102 | bezel_koko-aio_shaders-ng_colortools_and_ntsc_pass:frag, bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert |
| `exp2` | 89 | bezel_Mega_Bezel_shaders_guest_hsm-crt-dariusg-gdv-mini-potato:frag, bezel_Mega_Bezel_shaders_guest_hsm-crt-dariusg-gdv-mini:frag, bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-no-reflect:frag |
| `distance` | 71 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_text-adv-glass:frag |
| `greaterThanEqual` | 69 | bezel_Mega_Bezel_shaders_base_text-adv-glass:frag, bezel_Mega_Bezel_shaders_base_text-adv-no-reflect:frag, bezel_Mega_Bezel_shaders_base_text-adv:frag |
| `inverse` | 52 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_scanline-classic_shaders_bezel-hdr:frag |
| `fma` | 47 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `acos` | 37 | bezel_uborder_shaders_content_bezel_shaders_crt_crt-geom-bezel-reflections:frag, bezel_uborder_shaders_content_bezel_shaders_crt_crt-geom-bezel-reflections:vert, bezel_uborder_shaders_content_koko_ambi_crt_crt-geom:frag |
| `reflect` | 34 | crt_shaders_metacrt_bufC:frag, procedural_bergi-cave-quest:frag, procedural_ciberxtrem-portal-highquality:frag |
| `notEqual` | 30 | bezel_koko-aio_shaders-ng_fxaa:frag, edge-smoothing_sabr_shaders_sabr-hybrid-deposterize:frag, edge-smoothing_sabr_shaders_sabr-v3.0:frag |
| `tan` | 29 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `textureGather` | 29 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `intBitsToFloat` | 20 | edge-smoothing_nnedi3_shaders_nnedi3-nns128-win8x4-predict-h-luma:frag, edge-smoothing_nnedi3_shaders_nnedi3-nns128-win8x4-predict-h-rgb:frag, edge-smoothing_nnedi3_shaders_nnedi3-nns128-win8x4-predict-v-luma:frag |
| `fwidth` | 19 | bezel_koko-aio_shaders-ng_fxaa:frag, bezel_uborder_shaders_content_bezel_shaders_crt_crt-geom-bezel-reflections:frag, bezel_uborder_shaders_content_koko_ambi_crt_crt-geom:frag |
| `transpose` | 17 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, crt_crt-effects_shaders_royale-curve-append:vert |
| `floatBitsToUint` | 14 | crt_shaders_simple-crt_simple-crt:frag, crt_shaders_simple-crt_simple-crt:vert, edge-smoothing_fsr_shaders_fsr-pass0:frag |
| `isinf` | 14 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_color-hdr:frag |
| `isnan` | 14 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_color-hdr:frag |
| `modf` | 13 | crt_crt-effects_shaders_rt_curvature_rt_curvature:frag, edge-smoothing_xbr_other_presets_shaders_2xBR-multipass_2xbr-lv1-c-pass1:frag, edge-smoothing_xbr_shaders_xbr-lv2-multipass_xbr-lv2-pass1:frag |
| `determinant` | 12 | edge-smoothing_nedi_shaders_nedi-hybrid-pass0:frag, edge-smoothing_nedi_shaders_nedi-hybrid-pass0:vert, edge-smoothing_nedi_shaders_nedi-hybrid-pass1:frag |
| `asin` | 11 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert, bezel_scanline-classic_shaders_beam:frag |
| `equal` | 10 | crt_shaders_metacrt_Image:frag, crt_shaders_metacrt_bufC:frag, crt_shaders_metacrt_bufD:frag |
| `radians` | 10 | anamorphic_shaders_anamorphic:frag, bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam-mask:vert |
| `textureOffset` | 9 | crt_shaders_simple-crt_simple-color-correction:frag, edge-smoothing_scalefx_shaders_old_scalefx-pass0:frag, edge-smoothing_scalefx_shaders_old_scalefx-pass1:frag |
| `uintBitsToFloat` | 7 | crt_shaders_simple-crt_simple-crt:frag, crt_shaders_simple-crt_simple-crt:vert, edge-smoothing_fsr_shaders_fsr-pass0:frag |
| `refract` | 5 | procedural_dr2-rainbow-cavern:frag, procedural_dr2-wavescape:frag, procedural_dr2-white-folly:frag |
| `texelFetchOffset` | 4 | bezel_uborder_shaders_content_bezel_shaders_handheld_lcd-grid-v2-bezel-reflections:frag, bezel_uborder_shaders_content_bezel_shaders_handheld_lcd-grid-v2-bezel-reflections:vert, bezel_uborder_shaders_content_koko_ambi_handheld_lcd-grid-v2:frag |
| `texture2D` | 4 | crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt:frag, crt_shaders_cathode-retro_cathode-retro-crt-rgb-to-crt_no-signal:frag |
| `mod289` | 3 | misc_shaders_deband:frag, misc_shaders_deband:vert, misc_shaders_glass:frag |
| `packUnorm4x8` | 3 | edge-smoothing_scalenx_shaders_mmpx-ADV:frag, edge-smoothing_vectorscale_shaders_cell-rasterizer-multi-aa:frag, test_format:frag |
| `bitfieldExtract` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `bitfieldInsert` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `packHalf2x16` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `roundEven` | 2 | handheld_shaders_gameboy_shader-files_gb-pass1:frag, handheld_shaders_gameboy_shader-files_gb-pass1:vert |
| `trunc` | 2 | edge-smoothing_fsr_shaders_fsr-pass0:frag, edge-smoothing_fsr_shaders_fsr-pass1:frag |
| `unpackUnorm4x8` | 2 | edge-smoothing_vectorscale_shaders_cell-rasterizer-multi-aa:frag, test_decode-format:frag |
| `bitCount` | 1 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag |
| `findLSB` | 1 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag |
| `matrixCompMult` | 1 | edge-smoothing_xbr_shaders_jinc2-bilateral-xbr:frag |
| `noise1` | 1 | procedural_tdm-psx-rendering:frag |

## user-function

| construct | stages | examples |
|---|---:|---|
| `` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |

## spec

| construct | stages | examples |
|---|---:|---|
| `gl_FragCoord` | 199 | bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-no-reflect:frag, bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-no-reflect:vert, bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-ntsc-pass1:frag |
| `sampler-in-vertex` | 168 | anti-aliasing_shaders_aa-shader-4.0:vert, anti-aliasing_shaders_fxaa:vert, anti-aliasing_shaders_smaa_smaa-pass0:vert |

## spv-cap

| construct | stages | examples |
|---|---:|---|
| `Shader` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `ImageQuery` | 62 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |

## spv-extset

| construct | stages | examples |
|---|---:|---|
| `"GLSL.std.450"` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |

## spv-mode

| construct | stages | examples |
|---|---:|---|
| `OriginUpperLeft` | 1631 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |

## spv-dec

| construct | stages | examples |
|---|---:|---|
| `Binding` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `Block` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `ColMajor` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `DescriptorSet` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `Location` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `MatrixStride` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `Offset` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `BuiltIn` | 1752 | anamorphic_shaders_anamorphic:vert, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:vert |
| `Flat` | 67 | bezel_koko-aio_shaders-ng_ambi_temporal_pass:frag, bezel_koko-aio_shaders-ng_ambi_temporal_pass:vert, bezel_koko-aio_shaders-ng_avglum_pass:frag |
| `NoPerspective` | 34 | bezel_koko-aio_shaders-ng_ambi_temporal_pass:frag, bezel_koko-aio_shaders-ng_ambi_temporal_pass:vert, bezel_koko-aio_shaders-ng_avglum_pass:frag |
| `RelaxedPrecision` | 10 | crt_shaders_simple-crt_simple-color-correction:frag, crt_shaders_simple-crt_simple-color-correction:vert, crt_shaders_simple-crt_simple-crt:frag |

## spv-image

| construct | stages | examples |
|---|---:|---|
| `2D 0 0 0 1 Unknown` | 1562 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |

## spv-imgop

| construct | stages | examples |
|---|---:|---|
| `Lod` | 222 | anti-aliasing_shaders_smaa_smaa-pass1:frag, anti-aliasing_shaders_smaa_smaa-pass2:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `ConstOffset` | 20 | anti-aliasing_shaders_smaa_smaa-pass1:frag, bezel_Mega_Bezel_shaders_base_reflection-glass-hdr:frag, bezel_Mega_Bezel_shaders_base_reflection-glass:frag |
| `Bias` | 5 | border_shaders_ambient-glow:frag, border_shaders_effect-border-iq:frag, crt_shaders_cathode-retro_cathode-retro-crt-generate-screen-texture:frag |

## spv-loopctl

| construct | stages | examples |
|---|---:|---|
| `None` | 460 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |

## spv-std450

| construct | stages | examples |
|---|---:|---|
| `Floor` | 870 | anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag, auto-box_box-max:vert |
| `FMax` | 859 | annotated_passthru:vert, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `FMix` | 809 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `FClamp` | 773 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, anti-aliasing_shaders_reverse-aa:frag |
| `Pow` | 746 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `FAbs` | 657 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_aa-shader-4.0:frag |
| `FMin` | 613 | annotated_passthru:vert, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag |
| `Fract` | 556 | anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa:frag |
| `Sin` | 438 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `Sqrt` | 324 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_smaa_smaa-pass1:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `Exp` | 310 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-vert:frag |
| `Cos` | 281 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `Step` | 249 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass1:frag, bezel_Mega_Bezel_shaders_easymode_hsm-crt-easymode-halation:frag |
| `SmoothStep` | 219 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `Length` | 213 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `Ceil` | 204 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag, bezel_Mega_Bezel_shaders_base_cache-info-glass-params:frag |
| `Round` | 148 | anti-aliasing_shaders_smaa_smaa-pass1:frag, bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag, bezel_Mega_Bezel_shaders_base_cache-info-glass-params:frag |
| `SClamp` | 137 | bezel_scanline-classic_shaders_famicom-rgb:frag, bezel_scanline-classic_shaders_famicom-special:frag, crt_shaders_CreativeForce_crt-CreativeForce-Arcade:frag |
| `FSign` | 128 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_post-crt-prep-glass:frag |
| `Normalize` | 116 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `SMax` | 104 | bezel_scanline-classic_shaders_bezel-hdr:frag, bezel_scanline-classic_shaders_bezel-sdr:frag, bezel_scanline-classic_shaders_bezel-wcg:frag |
| `InverseSqrt` | 94 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `SMin` | 80 | crt_shaders_crt-beans_filter:frag, edge-smoothing_artcnn_shaders_artcnn-c4f16-conv2d-1:frag, edge-smoothing_artcnn_shaders_artcnn-c4f16-conv2d-2:frag |
| `Atan2` | 78 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `Acos` | 70 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `Cross` | 63 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `Log2` | 57 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag |
| `Exp2` | 49 | bezel_Mega_Bezel_shaders_guest_hsm-crt-dariusg-gdv-mini-potato:frag, bezel_Mega_Bezel_shaders_guest_hsm-crt-dariusg-gdv-mini:frag, bezel_Mega_Bezel_shaders_guest_hsm-crt-guest-advanced-no-reflect:frag |
| `Distance` | 41 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_text-adv-glass:frag |
| `Reflect` | 34 | crt_shaders_metacrt_bufC:frag, procedural_bergi-cave-quest:frag, procedural_ciberxtrem-portal-highquality:frag |
| `Log` | 28 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_koko-aio_shaders-ng_colortools_and_ntsc_pass:vert |
| `MatrixInverse` | 17 | bezel_Mega_Bezel_shaders_dogway_grade_orig:frag, bezel_Mega_Bezel_shaders_dogway_hsm-grade:frag, bezel_scanline-classic_shaders_color-hdr:frag |
| `Tan` | 17 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam:frag, bezel_scanline-classic_shaders_curve:frag |
| `Fma` | 16 | anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag, anti-aliasing_shaders_smaa_smaa-pass1:vert |
| `Modf` | 12 | crt_crt-effects_shaders_rt_curvature_rt_curvature:frag, edge-smoothing_xbr_other_presets_shaders_2xBR-multipass_2xbr-lv1-c-pass1:frag, edge-smoothing_xbr_shaders_xbr-lv2-multipass_xbr-lv2-pass1:frag |
| `Atan` | 10 | bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_reflection-glass-hdr:frag, bezel_Mega_Bezel_shaders_base_reflection-glass:frag |
| `Radians` | 9 | anamorphic_shaders_anamorphic:frag, bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam:frag |
| `UMax` | 6 | crt_shaders_crt-yah_crt-yah-single-pass:vert, crt_shaders_crt-yah_crt-yah:vert, crt_shaders_crt-yah_ntsc-pass1-horizontal:vert |
| `Asin` | 5 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_beam:frag, procedural_dr2-seabirds-at-sunset:frag |
| `Refract` | 5 | procedural_dr2-rainbow-cavern:frag, procedural_dr2-wavescape:frag, procedural_dr2-white-folly:frag |
| `SAbs` | 5 | reshade_shaders_bloom_BloomPass3:frag, reshade_shaders_bloom_BloomPass4:frag, reshade_shaders_bloom_LensFlarePass0:frag |
| `Determinant` | 3 | edge-smoothing_nedi_shaders_nedi-pass0:frag, edge-smoothing_nedi_shaders_nedi-pass1:frag, edge-smoothing_nedi_shaders_nedi-pass2:frag |
| `PackUnorm4x8` | 3 | edge-smoothing_scalenx_shaders_mmpx-ADV:frag, edge-smoothing_vectorscale_shaders_cell-rasterizer-multi-aa:frag, test_format:frag |
| `RoundEven` | 2 | handheld_shaders_gameboy_shader-files_gb-pass1:frag, handheld_shaders_gameboy_shader-files_gb-pass1:vert |
| `Tanh` | 2 | bezel_scanline-classic_shaders_limiter:frag, sharpen_shaders_adaptive-sharpen:frag |
| `UnpackUnorm4x8` | 2 | edge-smoothing_vectorscale_shaders_cell-rasterizer-multi-aa:frag, test_decode-format:frag |
| `FindILsb` | 1 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag |
| `PackHalf2x16` | 1 | edge-smoothing_fsr_shaders_fsr-pass1:frag |

## spv-op

| construct | stages | examples |
|---|---:|---|
| `OpCapability` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpDecorate` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpEntryPoint` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpExtInstImport` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpFunction` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpFunctionEnd` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpLabel` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpLoad` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpMemberDecorate` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpMemberName` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpMemoryModel` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpName` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpReturn` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpSource` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpSourceExtension` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpStore` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeFloat` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeFunction` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeMatrix` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypePointer` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeStruct` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeVector` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeVoid` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpVariable` | 3262 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpConstant` | 3257 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpTypeInt` | 3244 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpAccessChain` | 3155 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |
| `OpCompositeConstruct` | 2064 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, annotated_passthru:vert |
| `OpVectorShuffle` | 2034 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpConstantComposite` | 1976 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_aa-shader-4.0:frag |
| `OpMatrixTimesVector` | 1965 | anamorphic_shaders_anamorphic:vert, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:vert |
| `OpFMul` | 1919 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpTypeArray` | 1900 | anamorphic_shaders_anamorphic:vert, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:vert |
| `OpFAdd` | 1833 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag |
| `OpExtInst` | 1697 | anamorphic_shaders_anamorphic:frag, annotated_passthru:vert, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpExecutionMode` | 1631 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpTypeBool` | 1606 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, annotated_passthru:vert |
| `OpTypeImage` | 1562 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpTypeSampledImage` | 1562 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpFSub` | 1560 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag |
| `OpVectorTimesScalar` | 1549 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:vert |
| `OpFDiv` | 1537 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag |
| `OpBranchConditional` | 1513 | anamorphic_shaders_anamorphic:frag, annotated_passthru:vert, anti-aliasing_shaders_advanced-aa:vert |
| `OpBranch` | 1476 | anamorphic_shaders_anamorphic:frag, annotated_passthru:vert, anti-aliasing_shaders_advanced-aa:vert |
| `OpSelectionMerge` | 1447 | anamorphic_shaders_anamorphic:frag, annotated_passthru:vert, anti-aliasing_shaders_advanced-aa:vert |
| `OpCompositeExtract` | 1411 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpImageSampleImplicitLod` | 1378 | anamorphic_shaders_anamorphic:frag, annotated_passthru:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag |
| `OpFunctionCall` | 1341 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag |
| `OpFunctionParameter` | 1329 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag |
| `OpReturnValue` | 1326 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0:frag, anti-aliasing_shaders_ewa_curvature:frag |
| `OpFNegate` | 826 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag |
| `OpFOrdGreaterThan` | 790 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_shock:frag, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `OpFOrdLessThan` | 782 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag |
| `OpDot` | 625 | anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass1:frag, anti-aliasing_shaders_aa-shader-4.0-level2_aa-shader-4.0-level2-pass2:frag, anti-aliasing_shaders_aa-shader-4.0:frag |
| `OpLogicalNot` | 536 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `OpFOrdEqual` | 529 | anamorphic_shaders_anamorphic:frag, anti-aliasing_shaders_advanced-aa:vert, anti-aliasing_shaders_ewa_curvature:frag |
| `OpIAdd` | 510 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `OpPhi` | 482 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `OpSelect` | 480 | anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_smaa_smaa-pass1:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `OpConvertFToS` | 461 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `OpLoopMerge` | 460 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `OpFMod` | 429 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `OpConstantFalse` | 420 | anti-aliasing_shaders_fxaa:frag, bezel_Mega_Bezel_shaders_base_add-params-all:frag, bezel_Mega_Bezel_shaders_base_add-params-all:vert |
| `OpConvertUToF` | 418 | bezel_Mega_Bezel_shaders_base_intro-potato:frag, bezel_Mega_Bezel_shaders_base_intro:frag, bezel_Mega_Bezel_shaders_base_post-crt-prep-glass:frag |
| `OpConvertSToF` | 387 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `OpIEqual` | 374 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag |
| `OpUndef` | 374 | anti-aliasing_shaders_smaa_smaa-pass2:frag, auto-box_box-max:vert, bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag |
| `OpFOrdNotEqual` | 342 | annotated_passthru:frag, annotated_passthru:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |
| `OpSLessThan` | 306 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_fxaa:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag |
| `OpLogicalAnd` | 286 | anti-aliasing_shaders_fxaa:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `OpFOrdGreaterThanEqual` | 280 | anti-aliasing_shaders_fxaa:frag, bezel_Mega_Bezel_shaders_base_combine-passes-hdr:frag, bezel_Mega_Bezel_shaders_base_combine-passes-no-reflect-hdr:frag |
| `OpFOrdLessThanEqual` | 268 | anti-aliasing_shaders_smaa_smaa-pass0:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-vert:frag |
| `OpLogicalOr` | 256 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag |
| `OpVectorTimesMatrix` | 245 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `OpConstantTrue` | 242 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `OpISub` | 221 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_Mega_Bezel_shaders_base_text-adv-glass:frag |
| `OpImage` | 202 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `OpIMul` | 175 | bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag, bezel_Mega_Bezel_shaders_base_cache-info-glass-params:frag, bezel_Mega_Bezel_shaders_base_cache-info-no-reflect-params:frag |
| `OpImageFetch` | 143 | bezel_uborder_shaders_content_bezel_shaders_handheld_lcd-grid-v2-bezel-reflections:frag, bezel_uborder_shaders_content_koko_ambi_handheld_lcd-grid-v2:frag, crt_shaders_crt-beans_blur_horizontal:frag |
| `OpAll` | 138 | bezel_Mega_Bezel_shaders_base_cache-info-all-params:frag, bezel_Mega_Bezel_shaders_base_cache-info-glass-params:frag, bezel_Mega_Bezel_shaders_base_cache-info-no-reflect-params:frag |
| `OpSDiv` | 110 | crt_shaders_CreativeForce_crt-CreativeForce-Arcade:frag, crt_shaders_CreativeForce_crt-CreativeForce-SharpSmooth:frag, crt_shaders_monoCRT:frag |
| `OpSLessThanEqual` | 97 | anti-aliasing_shaders_ewa_curvature:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag |
| `OpSGreaterThanEqual` | 90 | bezel_Mega_Bezel_shaders_base_text-adv-glass:frag, bezel_Mega_Bezel_shaders_base_text-adv-no-reflect:frag, bezel_Mega_Bezel_shaders_base_text-adv:frag |
| `OpBitcast` | 88 | bezel_koko-aio_shaders-ng_colortools_and_ntsc_pass:frag, bezel_koko-aio_shaders-ng_final_pass:vert, bezel_koko-aio_shaders-ng_in_glow_y:vert |
| `OpImageSampleExplicitLod` | 86 | anti-aliasing_shaders_smaa_smaa-pass1:frag, anti-aliasing_shaders_smaa_smaa-pass2:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag |
| `OpMatrixTimesMatrix` | 77 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `OpBitwiseAnd` | 68 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `OpImageQuerySizeLod` | 62 | anti-aliasing_shaders_ewa_curvature:frag, bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag |
| `OpSGreaterThan` | 56 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_post-crt-prep-potato-megatron:frag |
| `OpConvertFToU` | 54 | bezel_Mega_Bezel_shaders_base_combine-passes-hdr:frag, bezel_Mega_Bezel_shaders_base_combine-passes-no-reflect-hdr:frag, bezel_Mega_Bezel_shaders_base_output-hdr:frag |
| `OpTranspose` | 50 | bezel_Mega_Bezel_shaders_base_bezel-images-over-crt:frag, bezel_Mega_Bezel_shaders_base_bezel-images-under-crt:frag, bezel_Mega_Bezel_shaders_base_blur-outside-screen-horiz:frag |
| `OpAny` | 47 | bezel_Mega_Bezel_shaders_mdapt_hsm-mdapt-pass0:frag, bezel_koko-aio_shaders-ng_ambi_temporal_pass:frag, bezel_koko-aio_shaders-ng_avglum_pass:vert |
| `OpUMod` | 47 | bezel_koko-aio_shaders-ng_final_pass:vert, bezel_koko-aio_shaders-ng_fxaa:vert, bezel_koko-aio_shaders-ng_in_glow_y:vert |
| `OpINotEqual` | 43 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_koko-aio_shaders-ng_fxaa:frag |
| `OpShiftRightArithmetic` | 41 | bezel_scanline-classic_shaders_3do-interpolation:frag, edge-smoothing_artcnn_shaders_artcnn-c4f16-depth-to-space:frag, edge-smoothing_artcnn_shaders_artcnn-c4f16-dn-depth-to-space:frag |
| `OpShiftRightLogical` | 28 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `OpSMod` | 23 | border_shaders_blur_fill_render_sampling_areas:frag, crt_crt-effects_shaders_rt_curvature_rt_curvature:vert, crt_shaders_CreativeForce_crt-CreativeForce-Arcade:frag |
| `OpUGreaterThan` | 22 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `OpImageGather` | 21 | crt_shaders_lottesRVM:frag, edge-smoothing_nnedi3_shaders_nnedi3-nns128-win8x4-predict-h-luma:frag, edge-smoothing_nnedi3_shaders_nnedi3-nns128-win8x4-predict-h-rgb:frag |
| `OpShiftLeftLogical` | 19 | bezel_scanline-classic_shaders_bezel-hdr:frag, bezel_scanline-classic_shaders_bezel-sdr:frag, bezel_scanline-classic_shaders_bezel-wcg:frag |
| `OpSwitch` | 19 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `OpSNegate` | 18 | anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass0:frag, anti-aliasing_shaders_reverse-aa-post3x_reverse-aa-post3x-pass1:frag, bezel_scanline-classic_shaders_decimate:frag |
| `OpBitwiseOr` | 17 | crt_shaders_simple-crt_simple-crt:frag, crt_shaders_simple-crt_simple-crt:vert, edge-smoothing_hqx_shaders_hq2x-halphon:frag |
| `OpDPdy` | 17 | anti-aliasing_shaders_ewa_curvature:frag, blurs_shaders_royale_blur10x10shared-gamma-encode-every-fbo:frag, blurs_shaders_royale_blur10x10shared-last-pass-gamma-encode-every-fbo:frag |
| `OpFwidth` | 16 | bezel_koko-aio_shaders-ng_fxaa:frag, bezel_uborder_shaders_content_bezel_shaders_crt_crt-geom-bezel-reflections:frag, bezel_uborder_shaders_content_koko_ambi_crt_crt-geom:frag |
| `OpMatrixTimesScalar` | 16 | bezel_Mega_Bezel_shaders_base_combine-passes-hdr:frag, bezel_Mega_Bezel_shaders_base_combine-passes-no-reflect-hdr:frag, crt_crt-effects_shaders_analog-service-menu:frag |
| `OpULessThan` | 16 | bezel_koko-aio_shaders-ng_avglum_pass:frag, bezel_koko-aio_shaders-ng_avglum_pass:vert, crt_shaders_cathode-retro_cathode-retro-decoder-composite-to-svideo:frag |
| `OpDPdx` | 15 | anti-aliasing_shaders_ewa_curvature:frag, blurs_shaders_royale_blur10x10shared-gamma-encode-every-fbo:frag, blurs_shaders_royale_blur10x10shared-last-pass-gamma-encode-every-fbo:frag |
| `OpLogicalEqual` | 15 | anti-aliasing_shaders_fxaa:frag, bezel_Mega_Bezel_shaders_fxaa_fxaa:frag, bezel_koko-aio_shaders-ng_fxaa:frag |
| `OpVectorExtractDynamic` | 15 | bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-no-reflect:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron-potato:frag, bezel_Mega_Bezel_shaders_megatron_crt-sony-megatron:frag |
| `OpKill` | 13 | anti-aliasing_shaders_smaa_smaa-pass0:frag, bezel_uborder_shaders_content_shaders_crt-aperture:frag, bezel_uborder_shaders_content_shaders_crt-easymode:frag |
| `OpBitwiseXor` | 12 | bezel_scanline-classic_shaders_bezel-hdr:frag, bezel_scanline-classic_shaders_bezel-sdr:frag, bezel_scanline-classic_shaders_bezel-wcg:frag |
| `OpUDiv` | 10 | bezel_koko-aio_shaders-ng_colortools_and_ntsc_pass:vert, bezel_scanline-classic_shaders_bezel-hdr:frag, bezel_scanline-classic_shaders_bezel-sdr:frag |
| `OpUGreaterThanEqual` | 9 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag, hdr_shaders_hdr10_v2:frag, hdr_shaders_hdr10_v2_config:frag |
| `OpIsInf` | 4 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_color-hdr:frag, bezel_scanline-classic_shaders_color-sdr:frag |
| `OpIsNan` | 4 | bezel_scanline-classic_shaders_beam-mask:frag, bezel_scanline-classic_shaders_color-hdr:frag, bezel_scanline-classic_shaders_color-sdr:frag |
| `OpLogicalNotEqual` | 2 | border_shaders_blur_fill_render_sampling_areas:frag, edge-smoothing_scalenx_shaders_mmpx-ADV:frag |
| `OpBitCount` | 1 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag |
| `OpNot` | 1 | edge-smoothing_vectorscale_shaders_resolve-crossings:frag |

## user-function in

| construct | stages | examples |
|---|---:|---|
| `out params` | 3259 | anamorphic_shaders_anamorphic:frag, anamorphic_shaders_anamorphic:vert, annotated_passthru:frag |

## user-function overloaded

| construct | stages | examples |
|---|---:|---|
| `` | 507 | anti-aliasing_shaders_smaa_smaa-pass0:frag, anti-aliasing_shaders_smaa_smaa-pass0:vert, anti-aliasing_shaders_smaa_smaa-pass1:frag |

## rejected by glslang today (not part of the dialect)

- read-fail: `./bezel/Mega_Bezel/shaders/base/phosphor-persistence.slang`
- read-fail: `./bezel/Mega_Bezel/shaders/easymode/hsm-crt-easymode-halation-basic-border.slang`
- fragment-fail: `./bezel/Mega_Bezel/shaders/megatron/inverse_tonemap.slang`
- fragment-fail: `./bezel/Mega_Bezel/shaders/newpixie/hsm-newpixie-crt.slang`
- fragment-fail: `./bezel/Mega_Bezel/shaders/scalefx/hsm-scalefx-pass4-hybrid.slang`
- read-fail: `./bezel/koko-aio/shaders-ng/includes/blooms.include.slang`
- read-fail: `./bezel/koko-aio/shaders-ng/includes/functions.include.slang`
- read-fail: `./bezel/koko-aio/shaders-ng/includes/functions.include_new.slang`
- read-fail: `./bezel/koko-aio/shaders-ng/includes/pack_unpack.include.slang`
- read-fail: `./bezel/koko-aio/shaders-ng/includes/pixel_glows.include.slang`
- read-fail: `./bezel/scanline-classic/shaders/bezel-base.slang`
- read-fail: `./bezel/scanline-classic/shaders/color-base.slang`
- read-fail: `./bezel/scanline-classic/shaders/menus/menu-base.slang`
- read-fail: `./bezel/scanline-classic/shaders/phosphor-chroma.slang`
- read-fail: `./bezel/scanline-classic/shaders/phosphor-luma.slang`
- vertex-fail: `./bezel/scanline-classic/shaders/yc-svideo.slang`
- vertex-fail: `./blurs/shaders/royale/blur5x5-gamma-encode-every-fbo.slang`
- vertex-fail: `./crt/shaders/mame_hlsl/shaders/old/shaders/mame_hlsl/shaders/mame_vector.slang`
- fragment-fail: `./crt/shaders/mame_hlsl/shaders/old/shaders/mame_hlsl/shaders/primary.slang`
- fragment-fail: `./hdr/shaders/inverse_tonemap.slang`
- fragment-fail: `./procedural/dr2-hexpo.slang`
- fragment-fail: `./procedural/shane-fractal-flythrough.slang`
