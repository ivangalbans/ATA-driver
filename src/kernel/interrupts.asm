; Handling interrupts requires some assembly code because we need to save
; every register to the stack in order to preserve the CPU state of the
; interrupted code, otherwise if the interrupt handler fails to restore the
; registers the interrupted code will probably fail once control is returned
; to it. However, to keep things simple, actual interrupt handling will be
; done in C, and for that we'll create a generic interrupt handler which we'll
; call for every single interrupt. Of course, this code will then pick the
; right handle to call, but that'll be done in C ;)
;
; However, there are a couple of other things to care about. If we're building
; a generic interrupt handler we'll need to know which interrupt actually took
; place. Well, Intel won't aid us here, because the only way to know which
; interrupt was raised is by knowing which code was executed. I mean, the code
; we register to handle IRQ 7 must know it's handling IRQ 7. Therefore, we
; need to create an interrupt handler for every possible interrupt which
; passes to the generic handler it's own IRQ. But we'll make use of a macro for
; this ;)
;
; Now, some insides from Intel's manuals.
;
; When the processor detects an interrupt it has two paths to follow, according
; to Intel's official documentation.
; 1. If the interrupt doesn't need a stack switch (i.e. when the CPL doesn't
;    change, which is the case when the interrupted code is the kernel itself
;    in our case). The rule is each ring has its own stack :)
;   1. Pushes EFLAGS, CS, EIP into the stack.
;   2. If the interrupt comes with an error code (i.e. IRQs 8, 10, 11, 12, 13,
;      14 and 17) it pushes this error code into the stack.
;   3. Loads from the interrupt or trap gate CS and EIP.
;   4. If the call is through an interrupt gate, clears IF to prevent other
;      interrupts from interrupting this handler.
;   5. Begins execution of the handler.
; 2. If the interrupt needs a stack switch (i.e. when the CPL of the currently
;    executing code is not the same as the handler's) then it must use a
;    different stack and a TSS must be set. And gdt.c has already handled that.
;   1. The processor will store in the current TSS its state at the moment of
;      the interrupt.
;   2. The processor checks the DPL of the vector and switches to the matching
;      stack defined in the TSS. Our interrupts and trap gates will always have
;      DPL 0 so TSS.ss0 and TSS.esp0 will the ones used.
;   3. Then in pushes into the new stack the previous SS and ESP, then EFLAGS,
;      CS and EIP. Since it came from a different PL the last two bits of CS
;      will hold the previous PL and that's how it knows whether to make a
;      task switch when returning from the handler.
;   3. From here on, it's the same as if no stack change has occurred, i.e.
;      step 1.3.
;
; Point 1.2 causes a difference in the stack due to the error code being pushed
; only in certain interrupts. In order to use one generic definition we'll add
; a fake 32 bits long error code to all interrupts not issuing an error code,
; thus the C code won't need to know which interrupts have errors :)
;
; Returning from an interrupt is initiated by issuing IRET. As before, IRET
; will behave differently if there was a stack switch or not.
; 1. With no stack switch:
;   1. Restores CS and EIP from the stack.
;   2. Restores the EFLAGS from the stack.
;   3. Increments EIP.
;   4. Resumes execution of the interrupted code.
; 2. With a stack switch it does the same plus restoring SS and ESP which
;    restores the previous stack. It also switches CPL based on the value
;    of the RPL in CS.
;
; So, to sum up, this is what will happen:
;   1. The interrupt comes and the specific little handler is called. This
;      little handler will put in the stack a fake error code if the interrupt
;      puts none -remember only some predefined interrupts issue error codes-
;      and then it'll push the IRQ it's serving. Then, it'll call a common
;      code.
;   2. The common code will store all registers and will call the C generic
;      handler. Before the call to C, the stack is as follows:
;       higher address  SS (if a stack switch occurred)
;                       ESP (if a stack switch occurred)
;                       EFLAGS
;                       CS
;                       EIP
;                       fake? error code
;                       IRQ
;                       General purpose regs (in PUSHAD order)
;   3. Once the C handler returns, the common code will restore the general
;      purpose registers, will pop both error code and IRQ and issue an IRET
;      to return from the handler to the interrupted code.
;
; It comes at a cost of several jumps and calls involved. The final workflow
; will be something like this:
;
;   INTERRUPTED CODE
;         |
;      IDT entry         (The architecture pushes things into the stack
;         |               depending on the values of this entry. Read above.)
;         |
;   specific ASM routine (See macros no_error_code_interrupt and
;         |               error_code_interrupt below. Depending on the IRQ
;         |               an extra fake error value will be pushed into the
;         |               stack.)
;         |
;   generic ASM routine  (See itr_common_part below. It will push the registers
;         \               into the stack and make all data segments point to
;          \              ss which should hold the kernel data selector.)
;           \
;     generic C handler  (See itr_interrupt_handler in interrupts.c. It will
;           /             inspect an array of pointers to functions with the
;          /              registered actual handlers.)
;         /
;   generic ASM routine  (See itr_common_part below. In this second part the
;        |                registers will be restored as well as the original
;        |                segments. Again, the old ss will used to fill all
;        |                data segments.)
;   INTERRUPTED CODE
;

[bits 32]
[extern itr_interrupt_handler]     ; This is the handler in C we'll call.

;;; Interrupt handling code ;;;

; Macro for errorless interrupts. Actually, the processor only pushes an error
; into the stack when handling interrupts 8, 10, 11, 12, 13, 14 and 17. Thus,
; this is the most common type of interrupt handler.
%macro no_error_code_interrupt 1
global itr_handler_%1                ; Declare the handler as global so it
                                      ; can be used from C.
itr_handler_%1:
  push 0x0                            ; Push fake error code.
  push %1                             ; Push IRQ
  jmp itr_common_part
%endmacro

; Macro for interrupt handlers with error code (i.e. IRQ 8, 10, 11, 12, 13, 14
; and 17).
%macro error_code_interrupt 1
global itr_handler_%1                ; Declare the handler as global so it
                                      ; can be used from C.
itr_handler_%1:
  push %1                             ; Push IRQ -remember the error code is
                                      ; already pushed.
  jmp itr_common_part
%endmacro

; Create all the 256 interrupt handler entry points. These are the actual
; routines that will be pointed at in the IDT. When I say "create" I mean all
; these 256 entry points will be generated here at compilation time, which
; means this all resides in the kernels .text segment.
%assign i 0
%rep 256
%if i = 8 || i = 10 || i = 11 || i = 12 || i = 13 || i = 14 || i = 17
error_code_interrupt i
%else
no_error_code_interrupt i
%endif
%assign i i+1
%endrep

; This is the common code all specific interrupts handler entry points will
; call.
itr_common_part:
  ; Save all registers in the stack in the following order:
  ;   eax, ecx, edx, ebx, esp, ebp, esi, edi
  pushad

  ; Before calling the C handler we must set all data segments to the same as
  ; SS, otherwise the data in the kernel won't be available.
  mov ax, ss
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  ; Call the C function. Following C calling convention, this function must
  ; have this signature:
  ;   itr_generic_handler(edi, esi, ebp, esp, ebx, edx, ecx, eax, irq, err,
  ;                       eip, cs, eflags)
  call itr_interrupt_handler

  ; Let's do the same checks the processor does: compare the CS stored in the
  ; stack to determine whether a new SS will be loaded or not. Our rule is:
  ; all our data segments are the same in a given ring.
  mov ax, cs
  mov bx, [esp + 44] ; This should be the previous CS.
  cmp ax, bx
  je data_segments_ready
  mov bx, [esp + 56] ; This should be the previous SS.
  mov ds, bx
  mov es, bx
  mov fs, bx
  mov gs, bx
  data_segments_ready:

  ; Restore the registers except for esp, which we won't actually need because
  ; it'll be restored after all pops.
  popad

  ; Pop both the IRQ and the error code.
  add esp, 8

  ; return from the handler.
  iretd


;;; IDT related routines ;;;

; Fill IDT entries with the corresponding handlers. This is needed to avoid
; repeating code like hell in C to use each of the 256 handler names to get
; their addresses to fill the IDT. So, this function will be called just to
; avoid that and it should be executed only once.
;
; It's C signature is:
;   void itr_set_idt_entries_offsets(void *idt_base_address);
;
; Since it'll be called from C with the IDT base address via short calls, the
; stack will look like:
;   [esp + 4]   # The IDT base address.
;   [esp    ]   # EIP.
;
; The address passed as argument must point to an array of 256 IDT entries.
; These entries' flags must be set somewhere else (e.g. the calling C code)
; because this function only writes the offsets to the entry points.
global itr_set_idt_entries_offsets
itr_set_idt_entries_offsets:
  %define arg_base_id_address esp + 4

  ; This macro will be used below just to use the nasm preprocessor instead of
  ; typing this 256 times.
  %macro idt_label 1
  mov dword eax, itr_handler_%1
  %endmacro

  ; Here starts the actual code of the function.
  mov dword ebx, [arg_base_id_address]
  %assign i 0
  %rep 256
  idt_label i         ; EAX = itr_handler_{i}
  mov word [ebx], ax  ; IDT.off_low = AX = itr_handler_{i}.off_low
  shr eax, 16         ; AX  = itr_handler_{i}.off_high
  add ebx, 6          ; EBX -> &(IDT.off_high)
  mov word [ebx], ax  ; IDT.off_high = AX = itr_handler_{i}.off_high
  add ebx, 2          ; EBX -> next IDT
  %assign i i+1
  %endrep
  ret

; Finally, the IDT must be loaded in assembly. Just like the LGDT, LIDT needs
; a 6 byte structure where the lower 2 bytes indicate the limit and the upper
; 4 indicate the base address.
;
; It's C signature is:
;   void itr_load_idt(void *)
;
; where the argument is the address to a six byte structure suitable to be
; loaded into LIDTR.
global itr_load_idt
itr_load_idt:
  %define arg_idtr_addr esp + 4

  mov eax, [arg_idtr_addr]
  lidt [eax]
  ret
