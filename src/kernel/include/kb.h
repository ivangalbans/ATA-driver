/* PS/2 Keyboard Driver.
 * We should convert the physical scan code set to an abstract one, but
 * I don't have time for that. Maybe in the future.
 */

#ifndef __KB_H__
#define __KB_H__

#include <interrupts.h>
#include <typedef.h>

typedef u8 kb_key_event;

int kb_init();

/* Keyboard interrupt handler. */
void kb_interrupt_handler(itr_cpu_regs_t,
                          itr_intr_data_t,
                          itr_stack_state_t);

/* This function will place in buf the next complete scan code. It will
 * return the size of the scan code. A return value of zero means there's
 * no available scan code. */
int kb_scan_code(char *buf);

#endif
