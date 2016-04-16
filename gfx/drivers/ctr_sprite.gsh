; Uniforms
;.fvec scale_vector
;.alias viewport_scale scale_vector.yxyx
;.alias texture_scale scale_vector.zwzw
.gsh
.fvec c0_

.constf c20_(1.0, 1.0, 1.0, 1.0)
.constf c21_(0.0, 0.0, 0.0, 0.0)
.constf c22_(0.0, 1.0, 0.0, 1.0)
.constf c23_(0.0, 0.0, -1.0, 1.0)

; Inputs
.alias sprite_coords v0
.alias tex_size      v1

.out o0_ position
.out o1_ color
.out o2_ texcoord0

.entry main_gsh
.proc main_gsh
   setemit 0
      mov o0_.xy, v0

      mov o0_.zw, c23_.zwzw
      mov o1_, c20_

      mov o2_, v1
   emit

   setemit 1
       mov o0_.x, v0
       mov o0_.y, v0.zwzw

       mov o0_.zw, c23_.zwzw
       mov o1_, c20_

       mov o2_.x, v1.zwzw
       mov o2_.yzw, v1.xyxy
   emit

   setemit 2, prim inv
       mov o0_.xy, v0.zw

       mov o0_.zw, c23_.zwzw
       mov o1_, c20_

       mov o2_, v1_.zwzw
   emit

   setemit 1, prim
       mov o0_.x, v0.zwzw
       mov o0_.y, v0

       mov o0_.zw, c23_.zwzw
       mov o1_, c20_

       mov o2_.x, v1
       mov o2_.yzw, v1.zwzw
   emit


   end
.end
