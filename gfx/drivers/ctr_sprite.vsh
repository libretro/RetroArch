; Uniforms
;.fvec scale_vector
;.alias viewport_scale scale_vector.yxyx
;.alias texture_scale scale_vector.zwzw
.fvec c0_

.constf c20_(1.0, 1.0, 1.0, 1.0)
.constf c21_(0.0, 0.0, 0.0, 0.0)
.constf c22_(0.0, 1.0, 0.0, 1.0)
.constf c23_(0.0, 0.0, -1.0, 1.0)

; Inputs
;.alias pos        v0
;.alias tex_coord  v1

.out o0_ position
.out o1_ texcoord0

.entry main_vsh
.proc main_vsh
   mul r0, c0_.yxyx, v0.yxwz
   add o0_, c20_, r0
   mul r1.zw, c0_.zwzw, v1.xyxy
   mov r1.xy, c21_
   add o1_, c22_, r1
   end
.end
