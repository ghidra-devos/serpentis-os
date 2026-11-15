cc

the C compiler of serpentis-os. runs inside the os, builds C programs there and builds itself there too

where it came from

not written from zero. its chibicc by Rui Ueyama, a small C compiler that is actually readable, MIT licensed, the license is in LICENSE.chibicc next to this file. i spent my time on the parts that touch my assembler, my kernel and my file format

what i did to it

cut it down and threw out everything that assumed linux
fasm style output for apple instead of gas
no linker. every .c plus the whole libc is parsed into one program and only what is reachable from serp_start gets written. the libc is compiled from source every time, its small enough
its own calling convention. arguments on the stack, last one first, result in rax, a double too as raw bits, structs through a hidden slot the caller passes. slower than registers, much less to get wrong
a peephole pass for the dumb stack machine stuff, push rax then pop rdi, jumps to the next line, store and load of the same slot
__syscall goes straight to int 0x80 so the libc needs no asm files
inside serpentis-os it spawns /bin/apple.serp on the result and waits

what it understands

structs and unions also by value, enums, typedef, function pointers, varargs, float and double, switch, goto, compound literals, designated initializers, asm, and a real preprocessor with function like macros, include, if ifdef elif and the rest

the files

tokenize.c tokens and keywords
preprocess.c include, define, conditionals, macro expansion
type.c types, sizes, conversions
parse.c the tree, type checks, constant folding
gen.c assembly, the peephole, putting the program together
main.c the driver, runs apple at the end

using it

cc hello.c
cc -o /bin/thing.serp a.c b.c
cc -S hello.c keeps the assembly
cc -v shows every step

building itself, as root because root owns /bin

cd /usr/src/serpentis/compiler
cc -v -o /bin/scc.serp main.c tokenize.c preprocess.c type.c parse.c gen.c

on the build machine clang compiles it first through tools/serpcc.bat or serpcc.sh, that copy is /bin/cc.serp. after that clang isnt needed

whats bad about it

the code it makes is slow, a stack machine with a peephole on top. compiling itself takes a while. error messages point at the right line but say little. i can read every line it makes with cc -S and for this project thats the point
