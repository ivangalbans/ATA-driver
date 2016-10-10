; This is just to give us freedom when writing the kernel in C, otherwise we
; would need to guarantee the main function is stored at the first address
; in the resulting binary. With this we can safely avoid such restriction.
; Of course, this will require the linker to link this and the kernel object
; file, and for that nasm will need to create an object file, not a plain,
; binary file.
;
; This step is required because we're building a bin kernel. If we learn how
; to load an ELF and set things properly for it to run then we could just get
; rid of this extra step. It'll be a lot cleaner and it's my guess we'll get
; a lot of beneficts from it. But, in the meantime, let's keep it like this.
;
; nasm -f elf -o kernel_entry.o kernel_entry.asm
;
; Then, we must assure this file is passed to the linker before the kernel.
;
;
[bits 32]
[extern kmain]

; This will be called with two arguments using a far call and kmain expects
; them both in the same position. Thus, let's remove the entries set by the
; far call.
add sp, 8
call kmain
hlt
