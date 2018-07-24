main: addi $1, $0, 10
addi $31, $0, 20
add $21, $1, $31
addi $v0, $0, 10
syscall

.data
.word 0x12345678
