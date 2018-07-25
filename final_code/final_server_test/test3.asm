main:	syscall
	addi $t1, $0, 0x7fff
	addi $t1, $t1, 0x7fff
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	add $t1, $t1, $t1
	addi $v0, $0, 10
	syscall
.data
pony:	.word 	0x12345678
