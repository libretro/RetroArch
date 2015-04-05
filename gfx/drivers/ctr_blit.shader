	.const c20, 0.0, 1.0, 0.0, 1.0

;	.in v0, vertex
;	.in v1, texCoord

	.out o0, result.position, 0xF
	.out o1, result.color, 0xF
	.out o2, result.texcoord0, 0x3

;	.uniform c0, c0, vertexScale
;	.uniform c1, c1, textureScale

	.vsh vmain, end_vmain

;code
	vmain:
		mul r0, c0, v0 (0x0)
		add o0, c20, r0 (0x1)
		mov o0, c20 (0x2)
		mov o1, c20 (0x3)
		mul r0, c1, v1 (0x4)
		add o2, c20, r0 (0x5)
		nop
		end
	end_vmain:

;operand descriptors
	.opdesc xyz_, xyzw, yxzw ; 0x0
	.opdesc xyz_, yyzw, xyzw ; 0x1
	.opdesc ___w, xyzw, xyzw ; 0x2
	.opdesc xyzw, wwww, wwww ; 0x3
	.opdesc xyzw, xyzw, xyzw ; 0x4
	.opdesc xyzw, xyzw, xyzw ; 0x5
