.text
.globl tigermain
.type tigermain, @function
tigermain:
pushl %ebp
movl %esp, %ebp
subl $0, %esp
L1:
movl $8, %edi
pushl %edi
call allocRecord
movl %eax, %edi
movl %edi, %esi
movl $4, %edi
movl %edi, 4(%esi)
movl $3, %edi
movl %edi, 0(%esi)
movl 0(%esi), %edi
pushl %edi
call printi
movl %eax, %edi
movl 4(%esi), %edi
pushl %edi
call printi
movl %eax, %edi
jmp L0
L0:

leave
ret

