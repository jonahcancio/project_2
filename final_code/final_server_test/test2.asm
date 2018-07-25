.text
main:	lw	$t1, 0x10010000($0)
	add	$t2, $t1, $t0
	addi	$v0, $0, 10
	syscall
.data
pony:	.word 	0x12345678
