main:	addi $18, $0, 0
	addi $19, $0, 4
loop:	beq $18, $19, escape
	addi $18, $18, 1
	j	loop
escape:	addi $v0, $0, 10
	syscall

.data
.word 0x12345678
