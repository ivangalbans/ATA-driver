; This loader will look for a file named /kernel in a MINIX filesystem, load it
; to 0x7e00 and jump to its first byte.
;
; It will take these very hard assumptions:
;   1. There is enough memory to hold the kernel at 0x7e00, which means the
;      kernel must be at most 608.5KB in length and, of course, there most
;      exists enough RAM to hold it. NO MEMORY CHECKING will be done here.
;      However, as a TODO, we could relocate this code to lower addresses and
;      load the kernel at a lower address to, leaving more room for the kernel
;      code and static data.
;   2. The kernel starts in 16-bit real mode. NO 32-bit PROTECTED MODE SWITCH
;      will be issued.
;   3. The sector the partition starts at fits in 32 bits.
;   4. Sector size if 512 bytes.

[org 0x0a00]
[bits 16]

;;; Memory allocation ;;;;
; We'll use the 29.75KB located below 0x7c00 to hold some structures we'll need
; as well as the stack:
;
;   0x0500 : disk packed structure used for calling INT 0x13, AH = 0x42.
;             struct disk_address_packed_structure {
;               uchar size_of_packet;
;               uchar always_zero;
;               uword number_of_sectors;
;               uword destination_offset;
;               uword destination_segment;
;               uint source_low_lba;
;               uint source_high_lba;
;             }
;  Let's call it Disk Address Packet Structure (DAPS)
DAPS_BASE               equ 0x0500
DAPS_SIZE_OF_PACKET     equ DAPS_BASE
DAPS_ALWAYS_ZERO        equ DAPS_BASE +  1
DAPS_NUMBER_OF_SECTORS  equ DAPS_BASE +  2
DAPS_DST_OFFSET         equ DAPS_BASE +  4
DAPS_DST_SEGMENT        equ DAPS_BASE +  6
DAPS_LBA_LOW            equ DAPS_BASE +  8
DAPS_LBA_HIGH           equ DAPS_BASE + 12
;
;   0x0510 : Space to hold the device we were loaded by BIOS at.
DEVICE                  equ 0x0510
;
;   0x0512 : Space to hold the initial block address of the inodes table.
INODES_TABLE_BLOCK      equ 0x0512
;
;   0x0514 : Space to hold the hex representation of a 32-bits integer. Its
;            format is 0x0000, thus it's 6 + 1 bytes long.
ASCII_BUFFER            equ 0x0514
;
;   0x051c : Space to hold an inode struct. Just an inode is enough since we'll
;            either handle the root inode or the kernel inode. The inode struct
;            is as follows:
;             typedef struct _minix_inode_t_ {
;               ushort i_mode;          /* Mode */
;               ushort i_uid;           /* Owner's ID */
;               uint i_size;            /* Size in bytes */
;               uint i_time;            /* Access time */
;               uchar i_gid;            /* Group's ID */
;               uchar i_nlinks;         /* Hard links */
;               ushort i_zone[9];       /* Zones */
;             }
INODE_BASE              equ 0x051c
INODE_MODE              equ INODE_BASE
INODE_UID               equ INODE_BASE +  2
INODE_SIZE              equ INODE_BASE +  4
INODE_TIME              equ INODE_BASE +  8
INODE_GID               equ INODE_BASE + 12
INODE_NLINKS            equ INODE_BASE + 13
INODE_ZONES             equ INODE_BASE + 14
;
;   0x053c : Minix superblock. Though the superblock has 1024 bytes reserved
;            in real life it's just a few bytes long. However, since we'll be
;            loading it from disk we'll need a whole sector to store it.
;            The superblock has the following structure:
;              struct _minix_sb_t_ {
;                ushort s_inodes;        /* Number of inodes. */
;                ushort s_nzones;        /* Number of zones. */
;                ushort s_imap_blocks;   /* Blocks used by inodes map. */
;                ushort s_zmap_blocks;   /* Blocks used by zones map. */
;                ushort s_firstdatazone; /* First block with file data. */
;                ushort s_log_zone_size; /* Size of zones area (2^x). */
;                uint s_max_size;        /* Max file size in bytes. */
;                ushort s_magic;         /* MINIX 14/30 ID number. */
;                ushort s_state;         /* Mount state. */
;              }
;
; Thus, let's define some macros to make this more readable.
SB_BASE                 equ 0x053c
SB_S_INODES             equ SB_BASE
SB_S_NZONES             equ SB_BASE +  2
SB_S_IMAP_BLOCKS        equ SB_BASE +  4
SB_S_ZMAP_BLOCKS        equ SB_BASE +  6
SB_S_FIRSTDATAZONE      equ SB_BASE +  8
SB_S_LOG_ZONE_SIZE      equ SB_BASE + 10
SB_S_MAX_SIZE           equ SB_BASE + 12
SB_S_MAGIC              equ SB_BASE + 16
SB_S_STATE              equ SB_BASE + 18
;
;   0x0550 : Current zone. When inspecting zones in a file this will hold the
;            current zone index.
CURRENT_ZONE_INDEX      equ 0x0550
;
;   0x0552 : Current zone in bytes. (CURRENT_ZONE_INDEX * 1024). 32 bits long.
CURRENT_ZONE_BYTES      equ 0x0552
;
;   0x0556 : Current direntry. When inspecting a directory this holds the
;            current direntry index in a given zone.
CURRENT_DIRENTRY_INDEX  equ 0x0556
;
;   0x0558 : Current direntry offset relative to the start of the block.
;            (CURRENT_DIRENTRY_INDEX * 32).
CURRENT_DIRENTRY_OFFSET equ 0x0558
;
;   0x055a : Current direntry absolute address.
;            (CURRENT_DIRENTRY_OFFSET + BUFFER_BASE)
CURRENT_DIRENTRY_ADDR   equ 0x055a
;
;   0x055c : "kernel" inode's inode table block.
KERNEL_INODE_BLOCK      equ 0x055c
;
;   0x055e : "kernel" inode's inode offset in KERNEL_INODE_BLOCK.
KERNEL_INODE_OFF        equ 0x055e
;
;   0x0600 : 1KB buffer. Read operations will be put here, then moved to their
;            right locations.
BUFFER_BASE             equ 0x0600
BUFFER_HALF             equ BUFFER_BASE + 512
;
;   0x0a00 : This code address when relocated. It's a 1KB space.
VBR_BASE                equ 0x0a00
VBR_HALF                equ VBR_BASE + 512
;
; - 0x0ffe : Stack.
STACK_TOP               equ 0x1000
;
;   0x1000 : Kernel area with DS = 0x0.
KERNEL_BASE             equ 0x1000

;;;; ENTRY POINT ;;;;

jmp short _relocate       ; This takes two bytes when decoded.

;;;; INSTALLATION RELATED FIELDS ;;;;

sector dd 0 ; This is the sector number of this sector when written to disk.
            ; We need to know this in order to properly identify the partition
            ; the system is installed in. Thus, the process of installing the
            ; bootloader consists on computing the sector it will be written
            ; at, altering this image by replacing the four bytes at offset 2
            ; with this sector number in little-endian and writting it to disk.

;;;; RELOCATION CODE ;;;;

_relocate:                ; The whole purpose of this code is to relocate this
  xor ax, ax              ; sector to a lower location. This will allow us to
  mov word ds, ax         ; later load the kernel at address 0x1000, thus
  mov word es, ax         ; giving us around 25KB extra space to store it.
  mov word si, 0x7c00
  mov word di, VBR_BASE
  mov word cx, 512
  rep movsb
  mov ax, main
  jmp ax


;;; STATIC AREA ;;;;

err_disk    db 'Disk error', 0
no_file_str db 'No /kernel found', 0
err_zone    db 'Bad zone number', 0
kernel_str  db 'kernel', 0
blah        db 'jumping to kernel', 0


;;;; FUNCTIONS ;;;;

; C signature.
;   void print_s(char *s);
print_str:
  ; These are our args and local vars.
  %define ps_arg_s  bp + 4
  ; Dynamic args must be computed.
  %define ps_s     bp - 2

  push bp
  mov bp, sp

  ; Add our activation registry.
  ;   char *fmt: pointer in format string @ bp - 2
  sub sp, 2

  mov word ax, [ps_arg_s]     ; Copy arg to local var.
  mov word [ps_s], ax         ; ps_s = ps_arg_s

  _print_str_loop:
    mov word si, [ps_s]                 ;
    lodsb                               ; Load DS:SI into AL.
    ; Initially I remember I coded this as
    ;   xor al, al
    ;   jz _print_str_done
    ; but for some reason NASM kept generating a JE instead a JZ. I had to
    ; change it in order to make it work. Some optimzation maybe? Or is it 8086
    ; had no JZ?
    cmp al, 0                           ; Test whether AL = 0.
    je _print_str_done                  ; Break if done.

    mov ah, 0xe                       ; Set BIOS VIDEO service's TeleType
                                      ; output routine code to print a
                                      ; single character.
    int 0x10                          ; Call BIOS VIDEO service.

    ; Move the pointer in the source string to the next char.
    mov word ax, [ps_s]                 ; ps_s ++
    inc ax
    mov word [ps_s], ax

    jmp _print_str_loop

  _print_str_done:
  ; Clear the stack.
  add sp, 2
  pop bp
  ret                                   ; return to caller.

; Load 2 sectors into BUFFER_BASE. Initially it was a more flexible function
; but setting all its arguments added up a lot of instructions and we need to
; make this code the smallest possible.
; In C, this will be it's signature:
;   void load_sectors(int lba)
; It halts the processor if an error is encountered.
load_sectors:
  ; There are our args and vars.
  %define ls_arg_lba   bp +  4    ; lba

  push bp
  mov bp, sp

  mov word dx, [DEVICE]       ; Load disk
  and dx, 0x00ff              ; Clear the high part just to avoid messing
                              ; things up.
  ; Fill the structure.
  mov dword [DAPS_LBA_HIGH], 0
  mov dword eax, [ls_arg_lba] ; Load lba
  mov dword [DAPS_LBA_LOW], eax
  mov word [DAPS_DST_SEGMENT], ds ; Load segment
  mov word [DAPS_DST_OFFSET], BUFFER_BASE ; Load offset
  mov word [DAPS_NUMBER_OF_SECTORS], 2 ; Load count
  mov byte [DAPS_ALWAYS_ZERO], 0
  mov byte [DAPS_SIZE_OF_PACKET], 16

  ; Load the rest of args for INT 0x13, AH = 0x42.
  xor eax, eax
  mov ah, 0x42
  mov si, DAPS_BASE
  int 0x13

  jc _load_sectors_failed ; Go to error handling if loading failed.

  ; Clear the stack and return.
  pop bp  ; Restore caller's bp.
  ret

  _load_sectors_failed:
    push word err_disk
    call print_str
    hlt

;; Compares two strings.
;; C signature:
;;   int strcmp(char *s1, char *s2);
;strcmp:
;  ; These are our vars and args.
;  %define st_arg_s1     bp + 4
;  %define st_arg_s2     bp + 6
;
;  push bp
;  mov bp, sp
;
;  mov word si, [st_arg_s1]
;  mov word di, [st_arg_s2]
;
;  _strcmp_loop:
;    mov byte al, [si]
;    mov byte bl, [di]
;    cmp al, bl
;    jl _strcmp_lt
;    jg _strcmp_gt
;    cmp al, 0
;    je _strcmp_eq
;    inc si
;    inc di
;    jmp _strcmp_loop
;
;  _strcmp_lt:
;    mov ax, -1
;    jmp _strcmp_done
;  _strcmp_gt:
;    mov ax, 1
;    jmp _strcmp_done
;  _strcmp_eq:
;    mov ax, 0
;  _strcmp_done:
;    pop bp
;    ret

; Main code.
main:
  cli             ; Clear interrupts so HALT is HALT.

  ;;; Step 1: Set up registers. At this point we know CS = 0x0, IP = 0x7c00
  ;;;         and DL = <device> (usually 0x80).
  xor ax, ax
  mov ds, ax      ; Data segment selector.
  mov es, ax      ; Second data segment selector (used by movsb).
  mov ss, ax      ; Stack segment selector.

  mov sp, STACK_TOP ; Let's place the stack right below this code. This will
  mov bp, sp        ; give us more than enough stack, about 29.75KB.

  mov word [DEVICE], dx  ; Store device so we can use DX. Though we only need
                         ; DL, let's also store DH in case we need it somehow.

  ;;; Step 2: Load second part of the bootlader into 0x7e00 and superblock into
  ;;;         its designed space.
  ;
  ; The general layout of a MINIX filesystem is:
  ;    Region         Offset (blocks)      Size (blocks)
  ;  --------------------------------------------------------------
  ;  | Bootloader   |        0           |           1            |
  ;  --------------------------------------------------------------
  ;  | Superblock   |        1           |           1            |
  ;  --------------------------------------------------------------
  ;  | Inodes map   |        2           | n = sb.s_imap_blocks   |
  ;  --------------------------------------------------------------
  ;  | Zones map    |      2 + n         | m = sb.s_zmap_blocks   |
  ;  --------------------------------------------------------------
  ;  | Inodes table |    2 + n + m       | (see below)            |
  ;  --------------------------------------------------------------
  ;  | Zones area   | sb.s_firstdatazone | (see below)            |
  ;  --------------------------------------------------------------
  ;
  ; Here, sb is the superblock itself, though it will be more evident below.
  ;
  ; Since the superblock starts at offset 1024 and we'll be loading one block
  ; (i.e. two sectors) from offset 512 then we'll collaterally get the first
  ; 512 bytes of the superblock. And since MINIX superblock is only 20 bytes
  ; long we'll be taking two birds down with just one rock.
  mov eax, [sector]
  inc eax
  push dword eax
  call load_sectors
  add sp, 4

  ; Move the second part of the bootleader to where it should be.
  mov word si, BUFFER_BASE
  mov word di, VBR_HALF
  mov word cx, 512
  rep movsb

  ; Move the 20-bytes superblock to where it should be.
  mov word si, BUFFER_HALF
  mov word di, SB_BASE
  mov word cx, 20
  rep movsb

  ;;; Step 3: Inspect the superblock and compute the starting block of the
  ;;;         inodes table. As seen previously, the inodes tables starts at:
  ;;;           i_table_first_block = 2 + sb.s_imap_blocks + sb.s_zmap_blocks
  mov ax, 2
  mov bx, [SB_S_IMAP_BLOCKS] ; sb.s_imap_blocks
  add ax, bx
  mov bx, [SB_S_ZMAP_BLOCKS] ; sb.s_zmap_blocks
  add ax, bx
  mov word [INODES_TABLE_BLOCK], ax

  ;;; Step 4: Load inode 1 (a.k.a root inode) to where it should be.
  ; Compute the absolute LBA of the inode table.
  mov word ax, [INODES_TABLE_BLOCK] ; Load the previously computed inodes table
  and eax, 0x0000ffff               ; Clear the higher part.
  shl eax, 1                        ; Multiply by two (block to sector).
  mov dword ebx, [sector]           ; Load the partition's first sector.
  add eax, ebx                      ; Add the partition's starting sector.
  push dword eax                    ; Push the table's absolute starting LBA.
  call load_sectors                 ; Load sectors.
  add sp, 4

  ; Copy the root inode to the global inode location.
  mov word si, BUFFER_BASE
  mov word di, INODE_BASE
  mov word cx, 32
  rep movsb

  ;;; Step 5: Inspect every zone in the root directory looking for a 'kernel'
  ;;;         entry.
  ; current_zone_index = 0;
  ; while (true) {
  ;   current_zone_bytes = current_zone_index * 1024;
  ;   if (file.length < current_zone_bytes) {
  ;     print_str("No file found");
  ;     hlt();
  ;   }
  ;   load_zone(current_zone_index);
  ;   current_direntry_index = 0;
  ;   while (true) {
  ;     current_direntry_offset = current_direntry_index * 32;
  ;     current_direntry_addr = current_direntry_offset + BUFFER_BASE;
  ;     if (file.length < current_direntry_offset + current_zone_bytes) {
  ;       print_str("No file found");
  ;       hlt();
  ;     }
  ;     if (*((uword *)current_direntry_addr) != 0 &&
  ;         strcmp(current_direntry_addr + 2, "kernel") == 0)
  ;       goto load_inode(*((uword *)current_direntry_addr));
  ;     current_direntry_index += 1;
  ;     if (current_direntry_index == 32)
  ;       break;
  ;   }
  ;   current_zone_index += 1;
  ; }
  mov word [CURRENT_ZONE_INDEX], 0        ; current_zone_index = 0

  _outer_loop:
    mov word ax, [CURRENT_ZONE_INDEX]     ; current_zone_bytes = ... * 1024
    and eax, 0x0000ffff
    shl eax, 10
    mov dword [CURRENT_ZONE_BYTES], eax


    mov dword ebx, [INODE_SIZE]           ; if file.length < current_zone_bytes
    cmp ebx, eax                          ;   file_not_found;
    jl _no_file_found

    push word [CURRENT_ZONE_INDEX]        ; load_zone(current_zone_index)
    call load_zone
    add sp, 2

    mov word [CURRENT_DIRENTRY_INDEX], 0  ; current_direntry_index = 0

  _inner_loop:
    mov word ax, [CURRENT_DIRENTRY_INDEX] ; current_direntry_offset =
    shl ax, 5                             ;   current_direntry_index * 32
    mov word [CURRENT_DIRENTRY_OFFSET], ax

    add ax, BUFFER_BASE                   ; current_direntry_addr =
    mov word [CURRENT_DIRENTRY_ADDR], ax  ;   current_dir_offset + BUFFER_BASE

    mov word ax, [CURRENT_DIRENTRY_OFFSET]
    and eax, 0x0000ffff                   ; if (file.length <
    mov dword ebx, [CURRENT_ZONE_BYTES]   ;     current_dir_offset +
    add eax, ebx                          ;     current_zone_bytes)
    mov dword ebx, [INODE_SIZE]           ;   file_not_found;
    cmp ebx, eax
    jl _no_file_found

    mov word ax, [CURRENT_DIRENTRY_ADDR]  ; if (*current_direntry_addr == 0)
    cmp ax, 0                             ;   skip_entry;
    je _inner_loop_continue               ;

    add ax, 2                             ; if (strcmp(current_direntry_addr
    push word ax                          ;            + 2, "kernel") != 0)
    push word kernel_str                  ;   skip_entry
    call strcmp
    add sp, 4
    cmp ax, 0
    jne _inner_loop_continue

    ; Here we found the "kernel" inode. It's stored at CURRENT_DIRENTRY_ADDR.
    jmp _step_6

  _inner_loop_continue:
    mov word ax, [CURRENT_DIRENTRY_ADDR]
    add ax, 2
    push word ax
    call print_str
    add sp, 2

    mov word ax, [CURRENT_DIRENTRY_INDEX] ; current_direntry_index ++
    inc ax
    mov word [CURRENT_DIRENTRY_INDEX], ax

    cmp ax, 32                            ; if current_direntry_index == 32
    jge _outer_loop_continue              ;   break;

    jmp _inner_loop                       ; end inner while.

  _outer_loop_continue:
    mov word ax, [CURRENT_ZONE_INDEX]     ; current_zone_index ++
    inc ax
    mov word [CURRENT_ZONE_INDEX], ax

    jmp _outer_loop                       ; end outer while.

  _no_file_found:
    push no_file_str
    call print_str
    hlt

;;; This code doesn't fit in 512 bytes.
times 510 - ($ - $$) db 0
db 0x55
db 0xaa

  ;;; Step 6: Load kernel inode.
_step_6:
  ; Compute location of kernel inode in inodes table.
  mov word bx, [CURRENT_DIRENTRY_ADDR]    ; CURRENT_DIRENTRY_ADDR holds the
  mov word ax, [bx]                       ; address where the inode number
  dec ax                                  ; is. However, in MINIX
                                          ; the inode table starts with inode 1.
  xor dx, dx                              ; Prepare for 16 division.
  mov word bx, 32                         ; Divide by inode size.
  div bx                                  ; DX:AX / BX => AX = quotient,
                                          ;               DX = remaninder
  mov word [KERNEL_INODE_BLOCK], ax       ; AX = inode table block index
  mov word [KERNEL_INODE_OFF], dx         ; DX = entry index in block above.

  ; Load kernel inode's containing block.
  mov word bx, [INODES_TABLE_BLOCK]       ; Load inode table's starting block.
  add ax, bx                              ; Add block index.
  shl ax, 1                               ; convert block offset to sector off.
  and eax, 0x0000ffff                     ; Clean relative sector upper part.
  mov dword ebx, [sector]                 ; Load first sector.
  add eax, ebx                            ; Compute absolute lba.
  push dword eax
  call load_sectors                       ; Load the whole block.
  add sp, 4

  ; Copy the kernel inode to INODE_BASE.
  mov word ax, [KERNEL_INODE_OFF]         ; Load inode's index.
  shl ax, 5                               ; Convert index to bytes.
  mov word bx, BUFFER_BASE                ; Inodes' table block is in buffer.
  add ax, bx                              ; Compute absolute address to inode.
  mov si, ax                              ; Copy inode.
  mov di, INODE_BASE
  mov cx, 32
  rep movsb

  ;;; Step 7: Load inode zones into consecutive memory.
  ; current_zone_index = 0;
  ; while (true) {
  ;   current_zone_bytes = current_zone_index * 1024;
  ;   if (file.length < current_zone_bytes) {
  ;     break;
  ;   }
  ;   load_zone(current_zone_index);
  ;   memcpy(BUFFER_BASE, KERNEL_BASE + current_zone_bytes, 1024);
  ;   current_zone_index += 1;
  ; }
  mov word [CURRENT_ZONE_INDEX], 0
  _outer_loop_2:
    mov word ax, [CURRENT_ZONE_INDEX]     ; current_zone_bytes =
    and eax, 0x0000ffff                   ;   current_zone_index * 1024
    shl eax, 10
    mov dword [CURRENT_ZONE_BYTES], eax

    mov dword ebx, [INODE_SIZE]           ; if (file.length <=
    cmp ebx, eax                          ;     current_zone_bytes)
    jle _outer_loop_2_done                ;   break;

    push word [CURRENT_ZONE_INDEX]        ; load_zone(current_zone_index)
    call load_zone
    add sp, 2

    ; Compute the values for ES:DI. Since the kernel can be larger than 64KB we
    ; can not use a single segment to store it. Thus, we must compute the right
    ; value for the segment. Actually, it's pretty simple. We're going to move
    ; only 1KB data and sectors are 64KB long, and they can start anywhere in
    ; memory every 16 bytes. Thus, by simply masking out the last four bits of
    ; the linear address and shifting this address four bits to the right we
    ; get a sector that almost starts at the desired linear address. Then we
    ; just need to use the masked-out four bits as the offset.
    mov dword eax, [CURRENT_ZONE_BYTES]   ; Compute the lineal address as
    mov word bx, KERNEL_BASE              ; KERNEL_BASE + CURRENT_ZONE_BYTES.
    and ebx, 0x0000ffff                   ; (extend BUFFER_BASE to 32 bits)
    add eax, ebx                          ; EAX holds the linear address.

    mov di, ax                            ; Transform address from lineal to
    and di, 0x000f                        ; logical by taking the last four
    and eax, 0x000ffff0                   ; bits as offset and the remaining
    shr eax, 4                            ; 16 bits as segment.
    mov es, ax                            ;

    mov word si, BUFFER_BASE              ; Set the source address.
    mov cx, 1024                          ; Set the counter.
    rep movsb                             ; Copy.

    mov word ax, [CURRENT_ZONE_INDEX]     ; CURRENT_ZONE_INDEX ++
    inc ax
    mov word [CURRENT_ZONE_INDEX], ax

    jmp _outer_loop_2

  _outer_loop_2_done:

  ;;; Step 8 : Call the kernel.
  mov word ax, KERNEL_BASE
  call ax

  ; And we shouldn't return here.
  hlt

; Loads a MINIX file zone from the current inode into BUFFER_BASE.
; Just as it happened with load_sectors, this was initially more flexible but
; space matters and we won't be handling more than one inode at a time.
; Signature in C:
;   void load_zone(uword zone);
; If an error is found it just halts.
load_zone:
  ; These are our args and vars.
  %define lz_arg_zone       bp + 4
  %define lz_level_0        bp - 2
  %define lz_level_1        bp - 4
  %define lz_level_2        bp - 6

  push bp
  mov bp, sp
  sub sp, 6

  ; Load file size.
  mov dword eax, [INODE_SIZE]

  ; Check whether zone is valid or not.
  mov word bx, [lz_arg_zone]
  and ebx, 0x0000ffff
  shl ebx, 10               ; * 1024
  cmp ebx, eax              ; if zone * 1024 >= size then zone is invalid.

  jge _lz_invalid_zone

  ; Compute zones.
  mov word ax, [lz_arg_zone]
  cmp ax, 519
  jge _lz_level_2
  cmp ax, 7
  jge _lz_level_1

  _lz_level_0:
    mov word [lz_level_2], -1
    mov word [lz_level_1], -1
    mov word [lz_level_0], ax
    jmp _lz_load_level_0
  _lz_level_1:
    mov word [lz_level_2], -1
    sub ax, 7
    mov word [lz_level_1], ax
    mov word [lz_level_1], 7 ; TODO: Check when I have a properly large file.
    jmp _lz_load_level_0
  _lz_level_2:
    sub ax, 519
    xor dx, dx
    mov word bx, 512
    div bx                ; DX:AX / 512 => DX : remainder, AX = quotient
    mov word [lz_level_2], dx
    mov word [lz_level_1], ax
    mov word [lz_level_0], 8
  _lz_load_level_0:
    ; Compute absolute lba
    mov word bx, [lz_level_0]   ; i
    shl bx, 1                   ; i * 2 (offset in i_zones)
    mov word ax, INODE_ZONES    ;
    add bx, ax                  ; inode + 14 + 2 * i (offset in mem)
    mov word ax, [bx]           ; inode.i_zones[i]
    and eax, 0x0000ffff         ; Pad to 32 bits.
    shl eax, 1                  ; blocks to lba (offset in partition)
    mov dword ebx, [sector]     ; load partition's first sector.
    add eax, ebx                ; sector + lba = absolute lba
    push dword eax              ; push lba
    call load_sectors           ; do load.
    add sp, 4
    mov word ax, [lz_level_1]   ; Check if we need to dive into level 1.
    cmp ax, -1
    je _lz_done
  _lz_load_level_1:
    ; Compute absolute lba
    mov word bx, [lz_level_1]   ; i
    shl bx, 1                   ; i * 2 (offset in block)
    mov word ax, BUFFER_BASE    ;
    add bx, ax                  ; block + i * 2 (offset in mem)
    mov word ax, [bx]           ; block[i]
    and eax, 0x0000ffff         ; Pad to 32 bits.
    shl eax, 1                  ; blocks to lba (offset in partition)
    mov dword ebx, [sector]     ; load partition's first sector.
    add eax, ebx                ; sector + lba = absolute sector.
    push dword eax              ; push lba
    call load_sectors           ; do load.
    add sp, 4
    mov word ax, [lz_level_2]   ; Check if we need to dive into level 2.
    cmp ax, -1
    je _lz_done
  _lz_load_level_2:
    ; Compute absolute lba
    mov word bx, [lz_level_2]   ; i
    shl bx, 1                   ; i * 2 (offset in block)
    mov word ax, BUFFER_BASE    ;
    add bx, ax                  ; block + i * 2 (offset in mem)
    mov word ax, [bx]           ; block[i]
    and eax, 0x0000ffff         ; Pad to 32 bits.
    shl eax, 1                  ; blocks to lba (offset in partition)
    mov dword ebx, [sector]     ; load partition's first sector.
    add eax, ebx                ; sector + lba = absolute sector.
    push dword eax              ; push lba
    call load_sectors           ; do load.
    add sp, 4
  _lz_done:
    add sp, 6
    pop bp
    ret
  _lz_invalid_zone:
    push word err_zone
    call print_str
    hlt

; Compares two strings.
; C signature:
;   int strcmp(char *s1, char *s2);
strcmp:
  ; These are our vars and args.
  %define st_arg_s1     bp + 4
  %define st_arg_s2     bp + 6

  push bp
  mov bp, sp

  mov word si, [st_arg_s1]
  mov word di, [st_arg_s2]

  _strcmp_loop:
    mov byte al, [si]
    mov byte bl, [di]
    cmp al, bl
    jl _strcmp_lt
    jg _strcmp_gt
    cmp al, 0
    je _strcmp_eq
    inc si
    inc di
    jmp _strcmp_loop

  _strcmp_lt:
    mov ax, -1
    jmp _strcmp_done
  _strcmp_gt:
    mov ax, 1
    jmp _strcmp_done
  _strcmp_eq:
    mov ax, 0
  _strcmp_done:
    pop bp
    ret
