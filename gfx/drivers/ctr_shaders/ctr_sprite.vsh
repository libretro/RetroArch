;.vsh
.entry main_vsh

; Uniforms
.fvec    scale_vector
.alias   viewport_scale scale_vector.yxyx
.alias   texture_scale scale_vector.zwzw

; Constants
.constf  _01N1   (0.0, 1.0, -1.0, 1.0)
.alias   _0000    _01N1.xxxx
.alias   _1111    _01N1.yyyy
.alias   _0101    _01N1.xyxy

; Inputs
.alias   pos_in         v0
.alias   texcoord_in    v1

; Output
.out     pos         position
.out     texcoord    texcoord0

.proc main_vsh

   mul   r0, viewport_scale, pos_in.yxwz
   add   pos, _1111, r0
   mul   r1, texture_scale, texcoord_in
   add   texcoord, _0101, r1

   end
.end
