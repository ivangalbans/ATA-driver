; MBR loader.
;
; When BIOS POST finishes, it looks at the first sector of some given devices.
; If it finds the mark 0xaa55 at positions 510-511 (the last two bytes of a
; 512-bytes sector), it will consider this device as bootable. Thus, it will
; load this sector into absolute address 0x7c00 and will transfer execution
; to that address.
;
; This code's main task is to read a standard MBR, pick the active partition,
; load it at 0x7c00 and jump to it.
;
; This code runs in real mode.
;
; CS:IP = 0x0:0x7c00.
; DL = device used to load this (usually 0x80)
;
; General x86 Real Mode Memory Map:
;   0x00000000 - 0x000003FF - Real Mode Interrupt Vector Table
;   0x00000400 - 0x000004FF - BIOS Data Area
;   0x00000500 - 0x00007BFF - Unused (29.75K)
;   0x00007C00 - 0x00007DFF - Bootloader code (512 bytes)
;   0x00007E00 - 0x0009FFFF - Unused (608.5K)
;   0x000A0000 - 0x000BFFFF - Video RAM (VRAM) Memory
;   0x000B0000 - 0x000B7777 - Monochrome Video Memory
;   0x000B8000 - 0x000BFFFF - Color Video Memory
;   0x000C0000 - 0x000C7FFF - Video ROM GBIOS
;   0x000C8000 - 0x000EFFFF - BIOS Shadow Area
;   0x000F0000 - 0x000FFFFF - System BIOS

[bits 16]
[org 0x0500]                            ; Make all addresses relative to 0x500
                                        ; cause we'll relocate ourselves here.
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax

; Relocate this whole sector to 0x0500. When we chainload to another VBR it
; can positively assume it's located at 0x7c00 and if we don't load it at
; this address we are the ones being WRONG. Ergo, we must move ourselves
; somewhere else because once we load this VBR into 0x7c00 we still need to
; jump there, so if we remain ourselves where the BIOS loaded us our code
; will be overwritten by our stupid self actions.
mov si, 0x7c00
mov di, 0x0500
mov cx, 0x200
rep movsb                             ; movsb moves a byte from ds:si to es:di
                                      ; then increments both si and di.
                                      ; rep does the action movsb, decs cx,
                                      ; until cx reaches 0.

; Set up a stack 512 bytes above (we know this area is free for sure). Just to
; avoid the stack gets randomly located when we issue some calls.
mov bp, 0x8000                        ; 512 bytes stack.
mov sp, bp                            ;

mov ax, main
call ax                              ; This is the magical touch! If main is
                                     ; used as in 'call main', nasm must place
                                     ; an immediate value, thus it'll compute
                                     ; it as relative to the next instruction
                                     ; regardless of the org. However, when
                                     ; main is used as an address, its value
                                     ; is computed using the org directive
                                     ; and issuing a call to a register is
                                     ; interpreted as an absolute address.

; GLOBAL VARIABLES
dev db 0                              ; Place to store physical drive if
                                      ; needed.


; Inspects a standard MBR partition table entry and if it's bootable it
; chainloads it.
;   AL holds the partition number.
inspect_std_mbr_entry:
  jmp inspect_std_mbr_init

  disk_address_packed_structure:
    size_of_packet    db      16      ; Size of packet
    always_zero       db       0      ; Always 0
    number_of_sectors dw       1      ; Transfer 1 sector
    dst_offset        dw  0x7c00      ; Transfer to 0x7c00
    dst_segment       dw       0      ; Transfer to segment
    low_lba           dd       0      ; Lower part starting LBA
    high_lba          dd       0      ; Upper part of 48 bits starting LBA

  part_no         db  0               ; Partition number.
  base_addr       dw  0               ; Entry base address.

  inspect_std_mbr_init:

  mov [part_no], al                   ; Store Partition Number.

  ; Compute entry address
  xor bx, bx
  mov bl, 16
  mul bl                              ; AX = AL * BL
  add ax, 0x0500                      ; AX += base address (relocated)
  add ax, 0x01be                      ; AX += partition table offset
  mov [base_addr], ax                 ; Store MBR entry base address

  ; Load and test status
  mov bx, [base_addr]                 ; Load entry address
  xor ax, ax                          ;
  mov al, [bx]                        ; Load status byte into AL
  and al, 0x80                        ; Check highest bit.
  jz inspect_std_mbr_entry_done

  ; Highest bit set, this partition is active. Ergo, let's fill the
  ; missing fields in the structure.
  mov eax, [bx + 8]                   ; Load starting LBA
  mov [low_lba], eax                  ; Fill the structure

  ; Call INT 0x13 con Extended Read (0x42)
  mov si, disk_address_packed_structure
  mov dl, [dev]
  mov ah, 0x42
  int 0x13

  jc inspect_std_mbr_entry_done
    mov ax, 0x7c00
    call ax

  inspect_std_mbr_entry_done:
  ret                                   ; return to caller.

; MAIN
main:
  ; Store the physical device in dev.
  mov [dev], dl

  mov ax, 0
  call inspect_std_mbr_entry
  mov ax, 1
  call inspect_std_mbr_entry
  mov ax, 2
  call inspect_std_mbr_entry
  mov ax, 3
  call inspect_std_mbr_entry

  hlt

; This code is commented out because partitioners will write the 0xaa55 mark
; after creating the MBR partition table.
;
; times 510 - ($ - $$) db 0             ; Pad first sector.
; dw 0xaa55                             ; Bootable mark.

; vim: syntax=asm
