	.file	"builtin.ll"
	.text
	.globl	printInt                        # -- Begin function printInt
	.p2align	4
	.type	printInt,@function
printInt:                               # @printInt
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	movl	%edi, %esi
	leaq	.L.str(%rip), %rdi
	movb	$0, %al
	callq	printf@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	printInt, .Lfunc_end0-printInt
	.cfi_endproc
                                        # -- End function
	.globl	printlnInt                      # -- Begin function printlnInt
	.p2align	4
	.type	printlnInt,@function
printlnInt:                             # @printlnInt
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	movl	%edi, %esi
	leaq	.L.str.1(%rip), %rdi
	movb	$0, %al
	callq	printf@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	printlnInt, .Lfunc_end1-printlnInt
	.cfi_endproc
                                        # -- End function
	.globl	getInt                          # -- Begin function getInt
	.p2align	4
	.type	getInt,@function
getInt:                                 # @getInt
	.cfi_startproc
# %bb.0:
	pushq	%rax
	.cfi_def_cfa_offset 16
	leaq	.L.str(%rip), %rdi
	leaq	4(%rsp), %rsi
	movb	$0, %al
	callq	scanf@PLT
	movl	4(%rsp), %eax
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end2:
	.size	getInt, .Lfunc_end2-getInt
	.cfi_endproc
                                        # -- End function
	.globl	exit                            # -- Begin function exit
	.p2align	4
	.type	exit,@function
exit:                                   # @exit
	.cfi_startproc
# %bb.0:
	retq
.Lfunc_end3:
	.size	exit, .Lfunc_end3-exit
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"%d"
	.size	.L.str, 3

	.type	.L.str.1,@object                # @.str.1
.L.str.1:
	.asciz	"%d\n"
	.size	.L.str.1, 4

	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym printf
	.addrsig_sym scanf
