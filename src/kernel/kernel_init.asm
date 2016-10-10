; This is the kernel's entry point. Since our bootloader doesn't switch to
; 32-bits protected mode we must do it ourselves. It also gives us the chance
; to perform several more actions. This code won't be linked to the main
; kernel code, it will be glued instead.
;
; The loaded kernel will use the memory as:
;   0x1000: kernel_init (this code)
;   0x1400: kernel_entry (a small code we know to start precisely at this
;           address which was linked to the main kernel files and thus knows
;           how to jump to it)
;   0x????: The main kernel code.
;
; We'll be also using free memory from 0x0500 to 0x1000 to hold the structures
; we need, at least temporarily, including the GDT and the memory map taken
; from BIOS.
;
; NOTE: We know the VBR loaded us at 0x1000. This is a very hard assumption.

[bits 16]
[org 0x1000]

;;;; MEMORY LAYOUT ;;;;
;
;   0x0500  : Memory to store the six bytes describing the GDT.
GDT_DESCRIPTOR          equ 0x0500
GDT_LENGTH              equ GDT_DESCRIPTOR
GDT_OFFSET              equ GDT_DESCRIPTOR + 2
;
;   0x0508  : GDT table. It will contain six entries, though we'll only use
;             two here. They are the null (required) entry, both kernel's
;             code and data segments, both user's code and data segments and
;             a TSS one. We'll also save some space for some extra entries we
;             won't probably ever use.
GDT_BASE                      equ 0x0508
GDT_NULL_SEGMENT              equ GDT_BASE
GDT_NULL_SEGMENT_HALF         equ GDT_NULL_SEGMENT + 4
GDT_KERNEL_CODE_SEGMENT       equ GDT_BASE + 8
GDT_KERNEL_CODE_SEGMENT_HALF  equ GDT_KERNEL_CODE_SEGMENT + 4
GDT_KERNEL_DATA_SEGMENT       equ GDT_BASE + 16
GDT_KERNEL_DATA_SEGMENT_HALF  equ GDT_KERNEL_DATA_SEGMENT + 4
GDT_END                       equ GDT_BASE + 24   ; This will be moved later
;                                                 ; when setting the user
;                                                 ; segments up.
;
;   0x0600  : Location to store the structures we'll get from the BIOS
;             describing the memory map. Let's hope it's not too large (~2K)
;             so it doesn't conflict with the small stack we'll set up here.
MEM_MAP_OFFSET    equ 0x0600
;
;   0x1400  : We know we'll load the kernel here and the stack bellow.
STACK_TOP         equ 0x1400
KERNEL_ENTRY      equ 0x1400

;;;; MAIN ;;;;

xor ax, ax                ; Clean things up.
mov word ds, ax
mov word es, ax
mov word gs, ax
mov word bp, STACK_TOP
mov word sp, STACK_TOP

;;;; LOAD MEMORY MAP ;;;;

; Ask the BIOS for a memory map. The kernel will receive this later as an
; argument. The last entry will be identified because all fields will be set
;  to 0.
; Entries returned by the BIOS are of the form:
;
;   0x00 (qword) : base address.
;   0x08 (qword) : size.
;   0x10 (dword) : type of range.
;     . 0x01 => memory, available to OS.
;     . 0x02 => reserved, not available.
;     . 0x03 => ACPI Reclaim Memory (usable after reading ACPI tables)
;     . 0x04 => ACPI NVS Memory (OS is required to save this memory between
;               NVS sessions) WTF?
;     . Everything else is undefined.

sti                                     ; Enable interrupts, otherwise we can't
                                        ; use BIOS services.

mmap_init:
  xor ebx, ebx                          ; First entry to request.
  mov word di, MEM_MAP_OFFSET           ; Store first entry at this place.
  and edi, 0x0000ffff                   ;  (Clean the upper part just in case)
  mov dword eax, 0xe820                 ; Get System Memory Map.
  mov dword edx, 'PAMS'                 ; Funny, but this is required.
  mov dword ecx, 20                     ; Entries are 20 bytes long.
  int 0x15                              ; Request the first entry.
  jc mmap_error                         ; Error.
  cmp eax, 'PAMS'                       ; Error
  jne mmap_error                        ;
  cmp ebx, 0                            ; End reached at the very beginning?
  je mmap_done                          ; Mmm... error.

mmap_test_entry:                        ;
  cmp ecx, 0                            ; If the returned entry's size is 0
  je mmap_next_entry                    ; skip it.

  mov dword ecx, [edi + 8]              ; Get the length field. Since this is
  cmp ecx, 0                            ; a 64-bit field we need to check it
  jne mmap_good_entry                   ; in two steps, one for the low part
  mov dword ecx, [edi + 14]             ; and another for the high part. It
  cmp ecx, 0                            ; seems it can happen there's an
  jne mmap_good_entry                   ; bogus entry with size 0, which we
                                        ; should skip.
  jmp mmap_next_entry                   ; If we got here, this one must be
                                        ; skipped because its length is 0.
mmap_good_entry:
  add edi, 20

mmap_next_entry:
  mov dword eax, 0xe820
  mov dword edx, 'PAMS'
  mov dword ecx, 20
  int 0x15
  jc mmap_done                          ; Some BIOses report the end of the
                                        ; the list by setting an error.
  cmp ebx, 0                            ; This is what we expect for such
  je mmap_done                          ; cases.
  jmp mmap_test_entry

mmap_error:                             ; If there was an error, let's blank
  xor al, al                            ; out the first entry. There's no
  mov word cx, 20                       ; point in providing the kernel with
  mov word di, MEM_MAP_OFFSET           ; unusable data.
  rep stosb
  jmp mmap_end

mmap_done:
  ; edi has the right value.
  xor al, al                            ; Clear the last entry.
  mov word cx, 20
  rep stosb

mmap_end:

;;;; SWITCH TO PROTECTED MODE ;;;;

;;;; Set the global descriptor table's entries ;;;;
;
; The code below prepare the GDT with two segments to properly
; switch to Protected Mode and, of course, the first null segment. The
; segments we'll create will be almost identical except for their types
; (code and data) and both will cover the whole possible space of 4GB from
; top to bottom. This is what Intel calls Basic Flat Model
; (IA-32 SDP, Vol. 3, 3.2.1)
;
; According to Intel, the GDT must be aligned to a 4 bytes boundary which is
; guarantee because it'll be located at 0x0500.
;
; GTD entries have a convoluted structure which is analyzed by the hardware
; with bit precision, therefore it's a little hard to handle. However, we
; just need to set a couple of them with very fixed values so we can just
; harcode what we need. An entry is a 64 bit structure with the following
; little-endian layout:
;
; 31-24 : Segment base address 31:24
; 23    : Granularity. 1 => use 4k pages.
; 22    : Default operation size. 0 => 16 bits, 1 => 32 bits.
; 21    : 64-bit code segment (IA-32e mode only).
; 20    : Available for use by system software.
; 19-16 : Segment limit 19:16.
; 15    : Present.
; 14-13 : Descriptor Privilege Level (0 - 3).
; 12    : Descriptor Type. 0 => System, 1 => Code or data.
; 11-08 : Segment type. It's a mask and each bit means:
;         Code: if 1 it's a code segment, data otherwise.
;         if Code == 1:
;           Conforming: if 0 a call from a lower privilege segment won't be
;                       allowed.
;           Readable: 1 means readable, 0 means the contrary.
;           Accessed: 0 means not accessed, 1 means accessed.
;         else:
;           Expansion direction: 1 means expand-down.
;           Write Enabled: 1 means writable.
;           Accessed: 1 means accessed.
; 07-00 : Segment base 23:16
; 31-16 : Segment base 15:00
; 15-00 : Segment limit 15:00
;
; The null entry.
xor eax, eax
mov dword [GDT_NULL_SEGMENT], eax
mov dword [GDT_NULL_SEGMENT_HALF], eax
; Code segment.
mov dword eax, 0x0000ffff               ; Base 15:00  = 0x0000
                                        ; Limit 15:00 = 0xffff
mov dword [GDT_KERNEL_CODE_SEGMENT], eax
mov dword eax, 0x00cf9a00               ; Base 31:24 = 0x00
                                        ; G|D/B|64|AVL = 0xc
                                        ;   Granularity = 1 => 4KB
                                        ;   D/B = 1 => 32 bits.
                                        ;   64-bit code segment = 0 => Nope.
                                        ;   AVL = 0 => (not used)
                                        ; Limit 19:16 = 0xf
                                        ; P|DPL|Type = 0x9
                                        ;   Present = 1
                                        ;   DPL = 00b (admin level)
                                        ;   Type = 1 (Code or data, not system)
                                        ; Segment type = 0xa
                                        ;   Code or data = 1 => Code
                                        ;   Conforming = 0
                                        ;   Readable = 1
                                        ;   Accessed = 0
                                        ; Base 23:16 = 0x00
mov dword [GDT_KERNEL_CODE_SEGMENT_HALF], eax
; Data segment.
mov dword eax, 0x0000ffff               ; Base 15:00  = 0x0000
                                        ; Limit 15:00 = 0xffff
mov dword [GDT_KERNEL_DATA_SEGMENT], eax
mov dword eax, 0x00cf9200               ; Base 31:24 = 0x00
                                        ; G|D/B|64|AVL = 0xc
                                        ;   Granularity = 1 => 4KB
                                        ;   D/B = 1 => 32 bits.
                                        ;   64-bit code segment = 0 => Nope.
                                        ;   AVL = 0 => (not used)
                                        ; Limit 19:16 = 0xf
                                        ; P|DPL|Type = 0x9
                                        ;   Present = 1
                                        ;   DPL = 00b (admin level)
                                        ;   Type = 1 (Code or data, not system)
                                        ; Segment type = 0x2
                                        ;   Code or data = 0 => Data
                                        ;   Expansion direction = 0 (expand up)
                                        ;   Write Enabled = 1
                                        ;   Accessed = 0
                                        ; Base 23:16 = 0x00
mov dword [GDT_KERNEL_DATA_SEGMENT_HALF], eax

; Store the GDT data in the GDT descriptor prepared for that.
mov word [GDT_LENGTH], GDT_END - GDT_BASE - 1
mov dword [GDT_OFFSET], GDT_BASE

cli                                     ; Clear interrupts because next
                                        ; actions are sensitive and can't
                                        ; be interrupted. Actually, if an
                                        ; interrupt takes place during the
                                        ; switch an unconsistent state can
                                        ; be reached and the architecture
                                        ; will crash.
lgdt [GDT_DESCRIPTOR]

mov eax, cr0                            ; cr0 controls several aspects of
or eax, 0x1                             ; current processor execution. See
mov cr0, eax                            ; Intel IA-32 SDM, Vol. 3, 2.5.
                                        ; Bit 0 sets Protected Mode.

; Segment selectors are offsets in bytes in the GDT. Let's use these two equ's
; to make it more readable.
KERNEL_CODE_SEGMENT     equ GDT_KERNEL_CODE_SEGMENT - GDT_BASE
KERNEL_DATA_SEGMENT     equ GDT_KERNEL_DATA_SEGMENT - GDT_BASE

jmp dword KERNEL_CODE_SEGMENT:init_pm   ; Issue a FAR JUMP in order to
                                        ; invalidate the current pipeline.

;;;; PROTECTED MODE ;;;;

; Now we are in protected mode, using 32 bits instructions.
[bits 32]

init_pm:

; At this point, all our segment register are meaningless. We need to update
; them to the actual values. The only one being right it CS because we far
; jumped here.
mov word ax, KERNEL_DATA_SEGMENT
mov word ds, ax
mov word ss, ax
mov word es, ax
mov word fs, ax
mov word gs, ax

; We must reset the stack because ESP can be pointing anywhere. We'll also be
; calling C code, thus we need a stack.
mov dword esp, STACK_TOP
mov ebp, esp

; Let's call the kernel entry function. It's C interface is something like:
;   void __kernel_entry(void * gdt_base, void * memory_map);
push dword MEM_MAP_OFFSET
push dword GDT_BASE
call KERNEL_CODE_SEGMENT:KERNEL_ENTRY

hlt

; Fill with zeros up to 1KB. This is required since after we glue the kernel
; with this piece of code we know where the kernel's entry point is located at.
times 0x400 - ($ - $$) db 0
