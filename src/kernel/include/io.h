/*
 * This file will hold all port-based I/O routines declarations.
 * However, the actual definitions will be scattered in other
 * files. Please, check the comments.
 */

#ifndef __IO_H__
#define __IO_H__

#include "typedef.h"

typedef u16 io_port_t;

/*
 * x86 OUT wrappers.
 * Actual definitions at src/kernel/io.asm.
 */
/* byte (8) */
void outb(io_port_t port, u8 value);
/* word (16) */
void outw(io_port_t port, u16 value);
/* double word (32) */
void outd(io_port_t port, u32 value);

/*
 * x86 IN wrappers.
 * Actual definitions as src/kernel/io.asm.
 */
/* byte (8) */
u8 inb(io_port_t port);
/* word (16) */
u16 inw(io_port_t port);
/* double word (32) */
u32 ind(io_port_t port);

#endif /* __IO_H__ */
