	.file	"hello.c"
	.section	.rodata
	.type	rcsid, @object
	.size	rcsid, 5
rcsid:
	.string	"$Id$"
	.globl	chen
	.data
	.align 4
	.type	chen, @object
	.size	chen, 4
chen:
	.long	1
	.align 4
	.type	yue, @object
	.size	yue, 4
yue:
	.long	5
	.section	.rodata
.LC0:
	.string	"%d %d %d \n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movl	$3, -4(%rbp)
	movl	meng.2181(%rip), %edx
	movl	chen(%rip), %eax
	movl	-4(%rbp), %ecx
	movl	%eax, %esi
	movl	$.LC0, %edi
	movl	$0, %eax
	call	printf
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.data
	.align 4
	.type	meng.2181, @object
	.size	meng.2181, 4
meng.2181:
	.long	2
	.ident	"GCC: (Ubuntu 4.8.2-19ubuntu1) 4.8.2"
	.section	.note.GNU-stack,"",@progbits
