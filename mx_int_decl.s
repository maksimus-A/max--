	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 13, 0	sdk_version 13, 3
	.globl	_mx_int_decl                    ; -- Begin function mx_int_decl
	.p2align	2
_mx_int_decl:                           ; @mx_int_decl
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #16						
	.cfi_def_cfa_offset 16				; reserve 16 bytes of stack space (sp 16 byte aligned)
	mov	w8, #1
	str	w8, [sp, #12]					; w8 = 32bit x8, store 1 to sp + 12 (x)
	mov	w8, #2				
	str	w8, [sp, #8]					; store 2 at sp + 8 (y)
	ldr	w8, [sp, #8]
	str	w8, [sp, #12]					; (y=x) load y from slot, store into x's slot.
	mov	w8, #3
	str	w8, [sp, #8]					; (y = 3) overwrite y with 3 
	ldr	w0, [sp, #12]
	add	sp, sp, #16						; load x into w0 (x0 32 bit), x0 is ret slot
	ret									; return to LR (link register)
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
