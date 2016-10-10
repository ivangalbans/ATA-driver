; These are the routines declared in hw.h.

global hw_hlt
global hw_cli
global hw_sti

hw_hlt:
  hlt
  ret

hw_sti:
  sti
  ret

hw_cli:
  cli
  ret
