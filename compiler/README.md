cc

the C compiler of serpentis-os. it runs inside the os, it builds C programs there, and it can build itself there too

where it came from

i didnt write this one from zero and im not going to pretend i did. a real C compiler is a monster, the parser alone is weeks of pain before you print hello world. so i took chibicc by Rui Ueyama. its a small C compiler written to be read and learned from, every step of it is clean and you can actually follow it, which is exactly why i picked it. its MIT licensed, the license is in LICENSE.chibicc next to this file and it stays there

i dont feel bad about it. thats how software works, gcc llvm and every libc out there are built on code and ideas from somebody else. i spent my time on the parts nobody could give me, making it talk to my assembler, my kernel and my file format

what i did to it

cut it down to what this system needs and threw out everything that assumed linux
the output is fasm style assembly that my own assembler apple understands, not gas
no linker. every .c you give it and the whole libc get parsed into one program, then only what is reachable from serp_start gets written. unused functions just never show up. the libc is compiled from source every time, its small enough that nobody notices
its own calling convention. all arguments go on the stack, the last one first, the result comes back in rax, even a double as raw bits. a struct gets copied into a hidden slot the caller passes. slower than passing in registers but there is way less that can go wrong
a peephole pass over the text that removes the dumb stuff a stack machine produces, push rax right before pop rdi, jumps to the next line, a store and a load of the same slot
__syscall goes straight to int 0x80 so the libc can talk to the kernel without any asm files
it runs inside serpentis-os, spawns /bin/apple.serp on the result and waits for it

what it understands

structs and unions also by value, enums, typedef, pointers to functions, varargs, float and double, switch, goto, compound literals, designated initializers, inline asm with asm, and a real preprocessor with function like macros, include, if ifdef elif and the rest

the files

tokenize.c cuts the text into tokens and knows the keywords
preprocess.c does include, define, the conditionals and macro expansion
type.c has the types, their sizes and the rules for mixing them
parse.c reads the tokens and builds the tree, checks types, folds constants
gen.c walks the tree and writes assembly, then the peephole, then puts the program together
main.c is the driver, reads the options, runs everything, then runs apple

using it

inside the os

cc hello.c
cc -o /bin/thing.serp a.c b.c
cc -S hello.c keeps the assembly so you can read what it made
cc -v shows every step

and to build the compiler with itself, as root because root owns /bin

cd /usr/src/serpentis/compiler
cc -v -o /bin/scc.serp main.c tokenize.c preprocess.c type.c parse.c gen.c

on the build machine its compiled with clang first through tools/serpcc.bat, that is the copy that ends up in /bin/cc.serp. after that it doesnt need clang anymore

whats bad about it

the code it makes is not fast. its a stack machine with a peephole on top, nothing more. compiling itself takes a while. error messages point at the right line but they are short. it works and i trust what it outputs because i can read every line of it with cc -S, and for this project thats the point
