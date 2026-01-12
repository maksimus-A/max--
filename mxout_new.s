	.text
	.p2align 2
	.global _main
_main:
	stp x29, x30, [sp, #-16]!
	mov x29, sp
	sub sp, sp, #112
	mov x8, #8
	str x8, [x29, #-8]
	mov x8, #3
	str x8, [x29, #-16]
	mov x8, #5
	str x8, [x29, #-24]
	mov x8, #4
	str x8, [x29, #-32]
	mov x8, #32
	str x8, [x29, #-40]
	mov x8, #234
	str x8, [x29, #-48]
	mov x8, #23
	str x8, [x29, #-56]
	mov x8, #234
	str x8, [x29, #-64]
	ldr x8, [x29, #-8]
	ldr x10, [x29, #-16]
	mul x11, x8, x10
	ldr x8, [x29, #-24]
	ldr x10, [x29, #-8]
	ldr x12, [x29, #-16]
	ldr x13, [x29, #-24]
	sdiv x14, x12, x13
	sub x12, x10, x14
	mul x10, x8, x12
	sub x8, x11, x10
	ldr x10, [x29, #-8]
	ldr x11, [x29, #-16]
	sub x12, x10, x11
	ldr x10, [x29, #-24]
	ldr x11, [x29, #-8]
	ldr x13, [x29, #-16]
	sdiv x14, x11, x13
	sub x11, x10, x14
	mul x10, x12, x11
	add x11, x8, x10
	str x11, [x29, #-72]
	ldr x8, [x29, #-72]
	add x10, x8, 1034
	str x10, [x29, #-80]
	ldr x8, [x29, #-80]
	add x10, x8, 5
	str x10, [x29, #-88]
	ldr x8, [x29, #-32]
	ldr x10, [x29, #-40]
	ldr x11, [x29, #-48]
	mul x12, x10, x11
	ldr x10, [x29, #-56]
	sdiv x11, x12, x10
	add x10, x8, x11
	ldr x8, [x29, #-64]
	sub x11, x10, x8
	ldr x8, [x29, #-32]
	ldr x10, [x29, #-40]
	add x12, x8, x10
	ldr x8, [x29, #-48]
	add x10, x12, x8
	ldr x8, [x29, #-56]
	add x12, x10, x8
	ldr x8, [x29, #-64]
	add x10, x12, x8
	mov x8, #13
	mul x12, x8, x10
	add x8, x11, x12
	str x8, [x29, #-72]
	ldr x8, [x29, #-72]
	mov x0, x8
b .Lreturn_func0
.Lreturn_func0:
	add sp, sp, #112
	ldp x29, x30, [sp], #16
	ret
