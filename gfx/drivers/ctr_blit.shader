; setup constants
   .const c20, 0.0, 1.0, 0.0, 1.0

; setup outmap
	.out o0, result.position, 0xF
	.out o1, result.color, 0xF
	.out o2, result.texcoord0, 0x3

; setup uniform map (not required)
	.uniform c0, c3, projection

   .vsh vmain, end_vmain

;code
	vmain:
		mov r0, v0 (0x4)
		mov r0, c20 (0x3)
		dp4 o0, c0, r0 (0x0)
		dp4 o0, c1, r0 (0x1)
		dp4 o0, c2, r0 (0x2)
		mov o0, c20 (0x3)
		mov o1, c20 (0x5)
		;mov o2, v1 (0x6)
		mul r0, c3, v1 (0x6)
		add o2, c20, r0 (0x7)
		flush
		nop
		end
	end_vmain:

;operand descriptors
	.opdesc x___, xyzw, xyzw ; 0x0
	.opdesc _y__, xyzw, xyzw ; 0x1
	.opdesc __z_, xyzw, xyzw ; 0x2
	.opdesc ___w, xyzw, xyzw ; 0x3
	.opdesc xyz_, xyzw, xyzw ; 0x4
	.opdesc xyzw, wwww, wwww ; 0x5
	.opdesc xyzw, xyzw, xyzw ; 0x6
	.opdesc xyzw, xyzz, xyzw ; 0x7
