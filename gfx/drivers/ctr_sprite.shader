	.const c20, 1.0, 1.0, 1.0, 1.0
	.const c21, 0.0, 0.0, 0.0, 0.0
	.const c22, 0.0, 1.0, 0.0, 1.0
	.const c23, 0.0, 0.0, -1.0, 1.0


;	.in v0, x0,y0,x1,y1
;	.in v1, tex_w,tex_h

	.out o0, result.position, 0xF
	.out o1, result.color, 0xF
	.out o2, result.texcoord0, 0x3

;	.uniform c0, c0, scale_vector

	.vsh main_vsh, endmain_vsh
	.gsh main_gsh, endmain_gsh

	main_vsh:
		mul r0, c0, v0 (0x1)
		add o0, c20, r0 (0x0)
		mul r1, c0, v1 (0x2)
		mov r1, c21 (0x3)
		add o1, c22, r1 (0x0)
		nop
		end
	endmain_vsh:

	main_gsh:
		setemit vtx0, false, false
			mov o0, v0 (0x3)

			mov o0, c23 (0x2)
			mov o1, c20 (0x0)

			mov o2, v1 (0x0)
		emit

		setemit vtx1, false, false
			mov o0, v0 (0x4)
			mov o0, v0 (0x8)

			mov o0, c23 (0x2)
			mov o1, c20 (0x0)

			mov o2, v1 (0x7)
			mov o2, v1 (0x9)
		emit

		setemit vtx2, true, true
			mov o0, v0 (0x6)

			mov o0, c23 (0x2)
			mov o1, c20 (0x0)

			mov o2, v1 (0xB)
		emit

		setemit vtx1, true, false
			mov o0, v0 (0x7)
			mov o0, v0 (0x5)

			mov o0, c23 (0x2)
			mov o1, c20 (0x0)

			mov o2, v1 (0x4)
			mov o2, v1 (0xA)
		emit

		nop
		end
	endmain_gsh:

; operand descriptors
	.opdesc xyzw, xyzw, xyzw ; 0x0
	.opdesc xyzw, yxyx, yxwz ; 0x1
	.opdesc __zw, zwzw, xyxy ; 0x2
	.opdesc xy__, xyzw, xyzw ; 0x3
	.opdesc x___, xyzw, xyzw ; 0x4
	.opdesc _y__, xyzw, xyzw ; 0x5
	.opdesc xy__, zwzw, zwzw ; 0x6
	.opdesc x___, zwzw, zwzw ; 0x7
	.opdesc _y__, zwzw, zwzw ; 0x8
	.opdesc _yzw, xyxy, xyxy ; 0x9
	.opdesc _yzw, zwzw, zwzw ; 0xA
	.opdesc xyzw, zwzw, zwzw ; 0xB
