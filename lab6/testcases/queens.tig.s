.text
.globl tigermain
.type tigermain, @function
tigermain:
pushl %ebp
movl %esp, %ebp
subl $20, %esp
L32:
movl $8, %edi
movl %edi, -4(%ebp)
movl $-8, %esi
movl %ebp, %edi
addl %esi, %edi
movl %edi, %esi
movl $0, %edi
pushl %edi
movl -4(%ebp), %edi
pushl %edi
call initArray
movl %eax, %edi
movl %edi, (%esi)
movl $-12, %esi
movl %ebp, %edi
addl %esi, %edi
movl %edi, %esi
movl $0, %edi
pushl %edi
movl -4(%ebp), %edi
pushl %edi
call initArray
movl %eax, %edi
movl %edi, (%esi)
movl $-16, %esi
movl %ebp, %edi
addl %esi, %edi
movl %edi, %esi
movl $0, %edi
pushl %edi
movl -4(%ebp), %edi
movl -4(%ebp), %edx
addl %edx, %edi
movl $1, %edx
subl %edx, %edi
pushl %edi
call initArray
movl %eax, %edi
movl %edi, (%esi)
movl $-20, %esi
movl %ebp, %edi
addl %esi, %edi
movl %edi, %esi
movl $0, %edi
pushl %edi
movl -4(%ebp), %edi
movl -4(%ebp), %edx
addl %edx, %edi
movl $1, %edx
subl %edx, %edi
pushl %edi
call initArray
movl %eax, %edi
movl %edi, (%esi)
movl $0, %edi
pushl %edi
pushl %ebp
call try
movl %eax, %edi
jmp L31
L31:

leave
ret

.text
.globl try
.type try, @function
try:
pushl %ebp
movl %esp, %ebp
subl $16, %esp
L34:
movl %esi, -4(%ebp)
movl %edi, -8(%ebp)
movl 12(%ebp), %esi
movl 8(%ebp), %edi
movl -4(%edi), %edi
cmp %edi, %esi
je L28
L29:
movl $0, %edi
movl %edi, -16(%ebp)
movl 8(%ebp), %edi
movl -4(%edi), %edi
movl $1, %esi
subl %esi, %edi
movl %edi, -12(%ebp)
movl -12(%ebp), %esi
movl -16(%ebp), %edi
cmp %esi, %edi
jg L13
L26:
movl 8(%ebp), %edi
movl -8(%edi), %ecx
movl $4, %edx
movl -16(%ebp), %edi
movl %edi, %esi
imul %edx, %esi
movl %ecx, %edi
addl %esi, %edi
movl (%edi), %esi
movl $0, %edi
cmp %edi, %esi
je L14
L15:
movl $0, %edi
movl %edi, %esi
L16:
movl $0, %edi
cmp %edi, %esi
jne L19
L20:
movl $0, %edi
movl %edi, %esi
L21:
movl $0, %edi
cmp %edi, %esi
jne L24
L25:
movl -12(%ebp), %esi
movl -16(%ebp), %edi
cmp %esi, %edi
jge L13
L27:
movl $1, %esi
movl -16(%ebp), %edi
addl %esi, %edi
movl %edi, -16(%ebp)
jmp L26
L28:
movl 8(%ebp), %edi
pushl %edi
call printboard
movl %eax, %edi
L30:
movl %edi, %eax
movl -4(%ebp), %edi
movl %edi, %esi
movl -8(%ebp), %edi
jmp L33
L14:
movl $1, %edi
movl %edi, %edx
movl 8(%ebp), %edi
movl -16(%edi), %ecx
movl 12(%ebp), %eax
movl -16(%ebp), %edi
movl %edi, %esi
addl %eax, %esi
movl $4, %edi
imul %edi, %esi
movl %ecx, %edi
addl %esi, %edi
movl (%edi), %esi
movl $0, %edi
cmp %edi, %esi
je L17
L18:
movl $0, %edi
movl %edi, %edx
L17:
movl %edx, %esi
jmp L16
L19:
movl $1, %edi
movl %edi, %edx
movl 8(%ebp), %edi
movl -20(%edi), %ecx
movl $7, %eax
movl -16(%ebp), %edi
movl %edi, %esi
addl %eax, %esi
movl 12(%ebp), %edi
subl %edi, %esi
movl $4, %edi
imul %edi, %esi
movl %ecx, %edi
addl %esi, %edi
movl (%edi), %esi
movl $0, %edi
cmp %edi, %esi
je L22
L23:
movl $0, %edi
movl %edi, %edx
L22:
movl %edx, %esi
jmp L21
L24:
movl $1, %ecx
movl 8(%ebp), %edi
movl -8(%edi), %eax
movl $4, %edx
movl -16(%ebp), %edi
movl %edi, %esi
imul %edx, %esi
movl %eax, %edi
addl %esi, %edi
movl %ecx, (%edi)
movl $1, %edx
movl 8(%ebp), %edi
movl -16(%edi), %ecx
movl 12(%ebp), %eax
movl -16(%ebp), %edi
movl %edi, %esi
addl %eax, %esi
movl $4, %edi
imul %edi, %esi
movl %ecx, %edi
addl %esi, %edi
movl %edx, (%edi)
movl $1, %edx
movl 8(%ebp), %edi
movl -20(%edi), %ecx
movl $7, %eax
movl -16(%ebp), %edi
movl %edi, %esi
addl %eax, %esi
movl 12(%ebp), %edi
subl %edi, %esi
movl $4, %edi
imul %edi, %esi
movl %ecx, %edi
addl %esi, %edi
movl %edx, (%edi)
movl 8(%ebp), %edi
movl -12(%edi), %edx
movl 12(%ebp), %edi
movl $4, %esi
imul %esi, %edi
movl %edx, %esi
addl %edi, %esi
movl -16(%ebp), %edi
movl %edi, (%esi)
movl 12(%ebp), %edi
movl $1, %esi
addl %esi, %edi
pushl %edi
movl 8(%ebp), %edi
pushl %edi
call try
movl %eax, %edi
movl $0, %ecx
movl 8(%ebp), %edi
movl -8(%edi), %eax
movl $4, %edx
movl -16(%ebp), %edi
movl %edi, %esi
imul %edx, %esi
movl %eax, %edi
addl %esi, %edi
movl %ecx, (%edi)
movl $0, %edx
movl 8(%ebp), %edi
movl -16(%edi), %ecx
movl 12(%ebp), %eax
movl -16(%ebp), %edi
movl %edi, %esi
addl %eax, %esi
movl $4, %edi
imul %edi, %esi
movl %ecx, %edi
addl %esi, %edi
movl %edx, (%edi)
movl $0, %edx
movl 8(%ebp), %edi
movl -20(%edi), %ecx
movl $7, %eax
movl -16(%ebp), %edi
movl %edi, %esi
addl %eax, %esi
movl 12(%ebp), %edi
subl %edi, %esi
movl $4, %edi
imul %edi, %esi
movl %ecx, %edi
addl %esi, %edi
movl %edx, (%edi)
jmp L25
L13:
movl $0, %edi
jmp L30
L33:

leave
ret

.text
.globl printboard
.type printboard, @function
printboard:
pushl %ebp
movl %esp, %ebp
subl $16, %esp
L36:
movl %ebx, %edx
movl %edx, -4(%ebp)
movl %esi, -8(%ebp)
movl %edi, -12(%ebp)
movl $0, %edi
movl %edi, -16(%ebp)
movl 8(%ebp), %edi
movl -4(%edi), %edi
movl $1, %esi
subl %esi, %edi
movl %edi, %esi
movl -16(%ebp), %edi
cmp %esi, %edi
jg L0
L10:
movl $0, %edi
movl 8(%ebp), %edx
movl -4(%edx), %edx
movl $1, %ecx
subl %ecx, %edx
movl %edx, %ebx
cmp %ebx, %edi
jg L1
L7:
movl 8(%ebp), %edx
movl -12(%edx), %edx
movl $4, %eax
movl -16(%ebp), %ecx
imul %eax, %ecx
addl %ecx, %edx
movl (%edx), %edx
cmp %edi, %edx
je L4
L5:
movl $L3, %edx
L6:
pushl %edx
call print
movl %eax, %edx
cmp %ebx, %edi
jge L1
L8:
movl $1, %edx
addl %edx, %edi
jmp L7
L4:
movl $L2, %edx
jmp L6
L1:
movl $L9, %edi
pushl %edi
call print
movl %eax, %edi
movl -16(%ebp), %edi
cmp %esi, %edi
jge L0
L11:
movl $1, %edx
movl -16(%ebp), %edi
addl %edx, %edi
movl %edi, -16(%ebp)
jmp L10
L0:
movl $L12, %edi
pushl %edi
call print
movl %eax, %edi
movl %edi, %eax
movl -4(%ebp), %edi
movl %edi, %ebx
movl -8(%ebp), %edi
movl %edi, %esi
movl -12(%ebp), %edi
jmp L35
L35:

leave
ret

.section .rodata
L12:
.int 1
.string "\n"

.section .rodata
L9:
.int 1
.string "\n"

.section .rodata
L3:
.int 2
.string " ."

.section .rodata
L2:
.int 2
.string " O"

