main: addi $1, $0, 10
or $31, $0, $1
sub $21, $1, $31
slt $2, $21, $31
addi $v0, $0, 10
syscall

.data
.word 0x12345678
