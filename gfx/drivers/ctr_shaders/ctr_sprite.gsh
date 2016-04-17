.gsh
.entry main_gsh

; Constants
.constf  _N1N1   (-1.0, 1.0, -1.0, 1.0)

; Inputs
.alias sprite_coords    v0
.alias tex_frame_coords v1    ;(0.0, 1.0, w/tex_w, 1.0 - h/tex_h)

.alias top_left            sprite_coords.xy ; real: top_right
.alias bottom_right        sprite_coords.zw ; real: bottom_left
.alias top_right           sprite_coords.zy ; real: bottom_right
.alias bottom_left         sprite_coords.xw ; real: top_left

.alias tex_top_left        tex_frame_coords.xy
.alias tex_bottom_right    tex_frame_coords.zw
.alias tex_top_right       tex_frame_coords.xw
.alias tex_bottom_left     tex_frame_coords.zy

; Outputs
.out  pos            position
.out  texcoord       texcoord0

.proc main_gsh
   setemit 0
      mov pos.xy, top_left.xy
      mov pos.zw, _N1N1
      mov texcoord.xy, tex_top_left.xy
   emit

   setemit 1
      mov pos.xy, bottom_left.xy
      mov pos.zw, _N1N1
      mov texcoord.xy, tex_bottom_left.xy
   emit

   setemit 2, prim inv
      mov pos.xy, bottom_right.xy
      mov pos.zw, _N1N1
      mov texcoord.xy, tex_bottom_right.xy
   emit

   setemit 1, prim
      mov pos.xy, top_right.xy
      mov pos.zw, _N1N1
      mov texcoord.xy, tex_top_right.xy
   emit

   end
.end
