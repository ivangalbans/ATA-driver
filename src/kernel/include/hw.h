/* This file holds the declarations of x86-specific routines that can't be
 * coded in C. The corresponding definitions are in hw.asm. */

#ifndef __HW_H__
#define __HW_H__

/* halt. */
void hw_hlt();

/* sti. */
void hw_sti();

/* cli. */
void hw_cli();

#endif
