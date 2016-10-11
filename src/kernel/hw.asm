; These are the routines declared in hw.h.
[bits 32]

global hw_hlt
global hw_cli
global hw_sti

; Invoke hlt.
hw_hlt:
  hlt
  ret

; Enable interrupts.
hw_sti:
  sti
  ret

; Disable interrupts.
hw_cli:
  cli
  ret
