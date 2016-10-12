/* Interrupt handling is divided in two files: interrupts.{c,asm}.
 * Since there are some issues that need to be handled in assembly code,
 * basically receiving the call, storing the registers, restoring them and
 * returning from the interrupt handler, we have no choice but to put this
 * part in assembly. However, everything set there will end up calling
 * intr_generic_handler() defined in interrupt.c.
 *
 * The whole picture is as follows:
 *
 * interrupts.asm   |       interrupts.c
 *                  |
 * IDT[0]   -----+  |                           +--- handlers[0]
 * IDT[1]   ---- +  |                           +--- handlers[1]
 *  ...          +---- itr_interrupt_handler ---+        ...
 * IDT[255] -----+  |                           +--- handlers[255]
 *
 *
 * The first two parts are static, i.e. they will be set up and they won't
 * be changed except for the flags in the IDT entries. This means the offsets
 * will remain. Changing the flags should not be needed, however.
 *
 * The reason why the first part is static is basically because the only
 * way to know the IRQ is by knowning in which entry in the IDT the interrupt
 * handler is placed, therefore it is ad-hoc knowledge. I honestly think this
 * should be more flexible and I've thought of a way to do it, but I have no
 * time right now. TODO: Find the time!
 *
 * There are further explanations in both implemetation files.
 */

#ifndef __INTERRUPTS_H__
#define __INTERRUPTS_H__

#include <typedef.h>

/* IDT flags. */
#define IDT_PRESENT               0x8000
#define IDT_NOT_PRESENT           0x0000
#define IDT_DPL_RING_0            0x0000
#define IDT_DPL_RING_1            0x2000
#define IDT_DPL_RING_2            0x4000
#define IDT_DPL_RING_3            0x6000
#define IDT_GATE_TASK             0x0500
#define IDT_GATE_INTR             0x0e00   /* 32 bits are assumed */
#define IDT_GATE_TRAP             0x0f00   /* 32 bits are assumed */

/* Nicer typedefs */
typedef u8 itr_irq_t;

/* This is just a nicer way to group the arguments.
 * Since we're using PUSHAD, this is the order in which the registers are
 * pushed into the stack. */
typedef struct {
  u32 edi;
  u32 esi;
  u32 ebp;
  u32 esp;
  u32 ebx;
  u32 edx;
  u32 ecx;
  u32 eax;
} __attribute__((__packed__)) itr_cpu_regs_t;

/* Interrupt-specific data */
typedef struct {
  u32 irq;
  u32 err;
} __attribute__((__packed__)) itr_intr_data_t;

/* Extra information set by the CPU when it called the handler. */
typedef struct {
  u32 eip;
  u32 cs;
  u32 eflags;
} __attribute__((__packed__)) itr_stack_state_t;

/* All interrupt handlers must follow this signature. Probably they'll never
 * need all this data, so probably we'll remove all this in the future.
 * Or maybe not. */
typedef void (*interrupt_handler_t)(itr_cpu_regs_t,
                                    itr_intr_data_t,
                                    itr_stack_state_t);


/* This is the API we'll use to set and remove interrupt handlers. */

/* Add this interrupt handler. It replaces the current handler without even
 * asking. If handler is NULL then nothing will be executed, though making
 * the handler available needs the right flags so it's not necessary to
 * take the handler out in case a temporary disabling is desired. */
void itr_set_interrupt_handler(itr_irq_t irq,
                               interrupt_handler_t handler,
                               u16 flags);

/* Set all the static part up. This should be called before expecting any
 * interrupt to be correctly handled. */
int itr_set_up();

#endif
