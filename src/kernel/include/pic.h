/* Header file for the driver for Intel's PIC 8295. */

#ifndef __PIC_H__
#define __PIC_H__

#include <interrupts.h>   /* This looks like introducing a cyclic dependency
                           * but we only include this header to get the data
                           * type itr_irq_t. */

/* These are the base IRQ we'll remap the PICs to. Intially the BIOS has set
 * up the PICs as well as the interrupts, but once we switch to protected
 * mode Intel reserves the first 32 IRQs to be used by exceptions. Thus, we
 * need to change the IRQs of the devices handled by the PICs. We know the
 * the meaning of each line in each PIC so we will remap them to the IRQs
 * immediately above the reserved ones. */
#define PIC_MASTER_BASE_IRQ        0x20
#define PIC_SLAVE_BASE_IRQ         0x28

/* Standard ISA ports */
enum pic_dev {
  PIC_TIMER_IRQ          = (PIC_MASTER_BASE_IRQ + 0),
  PIC_KEYBOARD_IRQ       = (PIC_MASTER_BASE_IRQ + 1),
  PIC_SLAVE_PIC_IRQ      = (PIC_MASTER_BASE_IRQ + 2),
  PIC_SERIAL_2_IRQ       = (PIC_MASTER_BASE_IRQ + 3),
  PIC_SERIAL_1_IRQ       = (PIC_MASTER_BASE_IRQ + 4),
  PIC_RESERVED_1         = (PIC_MASTER_BASE_IRQ + 5),
  PIC_FDD_IRQ            = (PIC_MASTER_BASE_IRQ + 6),
  PIC_LPT1_IRQ           = (PIC_MASTER_BASE_IRQ + 7),
  PIC_SPURIOUS_IRQ       = (PIC_MASTER_BASE_IRQ + 7),
  PIC_CMOS_RTC_IRQ       = (PIC_SLAVE_BASE_IRQ + 0),
  PIC_CGA_VERT_RET_IRQ   = (PIC_SLAVE_BASE_IRQ + 1),
  PIC_RESERVED_2         = (PIC_SLAVE_BASE_IRQ + 2),
  PIC_RESERVED_3         = (PIC_SLAVE_BASE_IRQ + 3),
  PIC_AUX_DEV_IRQ        = (PIC_SLAVE_BASE_IRQ + 4), /* PS/2 mouse */
  PIC_FPU_IRQ            = (PIC_SLAVE_BASE_IRQ + 5),
  PIC_PRIMARY_ATA_IRQ    = (PIC_SLAVE_BASE_IRQ + 6),
  PIC_SECONDARY_ATA_IRQ  = (PIC_SLAVE_BASE_IRQ + 7)
};

/* This is just a helper to keep the interface all drivers have. It actually
 * calls pic_remap() and set sets all  */
void pic_init();

/* This will set the PIC to the configuration we want. It's not a generic
 * reset, it's just our way to do this. Basically it does what we said in the
 * comment above about the reserved IRQs. */
void pic_remap();

/* Send end-of-interrupt to the PICs. It must be called after the interrupt
 * handler is done with the interrupt. */
void pic_send_eoi(itr_irq_t irq);

/* Deactivates (aka. mask) the device line in the PIC. This will cause the PIC
 * to "silence" the interrupt raised by a given device. */
void pic_mask_dev(enum pic_dev dev);

/* Activates (aka. unmask) the device line in the PIC. After calling this the
 * interrupts raised by the device will cause the PIC to interrupt the
 * processor. */
void pic_unmask_dev(enum pic_dev dev);

#endif /* __PIC_H__ */
