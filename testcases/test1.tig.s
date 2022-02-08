.text
.global tigermain
.type tigermain, @function
tigermain:
pushl %ebp
movl %esp, %ebp
subl $64, %esp
pushl %ebx
pushl %esi
pushl %edi
leal -4(%ebp), %ebx
movl $0, %ecx
pushl %ecx
movl $10, %edx
pushl %edx
call initArray
addl $8, %esp
movl %eax, 0(%ebx)
movl -4(%ebp), %eax
jmp L0
L0:

popl %edi
popl %esi
popl %ebx
leave
ret
