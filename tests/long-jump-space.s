#Option:  --mao=RELAX=collect_stats[1] --mao=TEST
#grep MaoRelax.*expr.*134 1
#
# On x86_64, the following code, the jump should be 5 bytes, since the offset
# is longer than 128 bytes.
#
.globl expr
	.type	expr, @function
expr:
	jmp	FOO
	.space 128
FOO:
	ret
	.size	expr, .-expr
