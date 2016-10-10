SECTIONS {
  . = 0x1400;
  .text : { *(.text) }
  .data : { *(.data) }
  .bss  : { *(.bss) }
}
