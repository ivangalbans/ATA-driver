[bits 32]

; Relocates the stack to the given address. The function calling this must
; have no activation registry since the only guarantee we provide is to return
; to the caller. The current stack's history will be lost from this point on
; and a new one will be set.
;
; This function should be called in a C-like form and it receives a 32-bits
; integer, which will be the address to set the stack pointer to. It must be
; a short call, not a far one.
;
; The signature of this function is C would be:
;   void mem_relocate_stack_to(void *);
;
[global mem_relocate_stack_to]
mem_relocate_stack_to:
  ; Following C call convention, we should have a stack set this way.
  ;   [esp + 4] holds the stack's new base address.
  ;   [esp    ] holds the IP to return to.
  %define arg_stack_base  esp + 4
  %define __caller_addr_  esp
  mov dword eax, [__caller_addr_]   ; EAX holds the EIP to return to.
  mov dword esp, [arg_stack_base]   ; Move ESP to the new value. From this
                                    ; point on the old stack is lost.
  mov dword ebp, esp                ; Set EBP to the new ESP.
  push dword 0x0                    ; When this function returns ESP must be
  push dword eax                    ; pointing to the address specified. Since
                                    ; RET will take EIP off the stack and the
                                    ; C calling convention will also take the
                                    ; argument off, we need to set up there two
                                    ; holes to avoid messing with data above.
  ret                               ; Return.
