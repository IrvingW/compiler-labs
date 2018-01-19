.text
.globl tigermain
.type tigermain, @function
tigermain:
pushl %ebp
movl %esp, %ebp
subl $4, %esp
L4:
movl $4, -4(%ebp)
movl $0, %edi
pushl %edi
pushl %ebp
call add
movl %eax, %edi
jmp L3
L3:

leave
ret

.text
.globl add
.type add, @function
add:
pushl %ebp
movl %esp, %ebp
subl $0, %esp
L6:
movl 12(%ebp), %ecx
movl 8(%ebp), %edx
movl -4(%edx), %edx
cmp %edx, %ecx
je L0
L1:
movl 12(%ebp), %edx
movl $1, %ecx
addl %ecx, %edx
pushl %edx
movl 8(%ebp), %edx
pushl %edx
call add
movl %eax, %edx
L2:
movl %edx, %eax
jmp L5
L0:
movl 12(%ebp), %edx
pushl %edx
call printi
movl %eax, %edx
jmp L2
L5:

leave
ret

