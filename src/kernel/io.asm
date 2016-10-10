; This are the actual implementations of the port-based I/O routines
; defined in src/includes/io.h. They all follow the C-style argument
; passing to make them compatible with the wrapping declarations made
; there, i.e. the arguments are placed in the stack in reverse order.
; For the outX family, this means:
;   [esp + 8] holds the value.
;   [esp + 4] holds the port.
;   [esp]     holds the return address.
; For the inX family, this means:
;   [esp + 4] holds the port
;   [esp    ] holds the return address.
;   al, ax, eax will hold the retrieved value according to the case.

[bits 32]
global outb
global outw
global outd
global inb
global inw
global ind

outb:
  mov al, [esp + 8]
  mov dx, [esp + 4]
  out dx, al
  ret

outw:
  mov ax, [esp + 8]
  mov dx, [esp + 4]
  out dx, ax
  ret

outd:
  mov eax, [esp + 8]
  mov dx, [esp + 4]
  out dx, eax
  ret

inb:
  mov dx, [esp + 4]
  in al, dx
  ret

inw:
  mov dx, [esp + 4]
  in ax, dx
  ret

ind:
  mov dx, [esp + 4]
  in eax, dx
  ret
