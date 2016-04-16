.gsh
.entry main_gsh

; Constants
.constf  _01N1   (0.0, 1.0, -1.0, 1.0)
.alias   _0000    _01N1.xxxx
.alias   _1111    _01N1.yyyy
.alias   _0101    _01N1.xyxy
.alias   _N1N1    _01N1.zwzw

; Inputs
.alias sprite_coords v0
.alias tex_size      v1
.alias top_left      sprite_coords.xyxy
.alias bottom_right  sprite_coords.zwzw

; Outputs
.out  pos            position
.out  texcoord       texcoord0

.proc main_gsh
   setemit 0
      mov pos.xy, top_left.xy
      mov pos.zw, _N1N1
      mov texcoord.xy, tex_size.xy
   emit

   setemit 1
      mov pos.x, top_left.x
      mov pos.y, bottom_right.y
      mov pos.zw, _N1N1
      mov texcoord.x, tex_size.z
      mov texcoord.y, tex_size.y
   emit

   setemit 2, prim inv
      mov pos.xy, bottom_right.xy
      mov pos.zw, _N1N1
      mov texcoord.xy, tex_size.zw
   emit

   setemit 1, prim
      mov pos.x, bottom_right.x
      mov pos.y, top_left.y
      mov pos.zw, _N1N1
      mov texcoord.x, tex_size.x
      mov texcoord.y, tex_size.w
   emit

   end
.end
