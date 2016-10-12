/* This is the driver for Intel's PIC (8295).
 * TODO: Describe this better. */

#include <pic.h>
#include <io.h>
#include <fb.h>
#include <string.h>
#include <typedef.h>

/* PICs ports. */
#define PIC_MASTER_CMD_PORT     0x20
#define PIC_MASTER_DATA_PORT    0x21
#define PIC_SLAVE_CMD_PORT      0xa0
#define PIC_SLAVE_DATA_PORT     0xa1

/* PIC commands. */
#define PIC_EOI                 0x20 /* Enf-of-interrupt. This tells the PIC
                                      * the interrupt has been handled. */

/* Initialization commands (Initialization Command Word N) bits.*/
#define PIC_ICW1_ICW4           0x01 /* ICW4 needed / ICW4 not needed */
#define PIC_ICW1_SINGLE         0x02 /* Single mode / Cascade mode */
#define PIC_ICW1_INTERVAL4      0x04 /* Call address interval: 4 / 8 */
#define PIC_ICW1_LEVEL          0x08 /* Triggered mode: Level / Edge */
#define PIC_ICW1_INIT           0x10 /* Initialization command. */

#define PIC_ICW4_8086           0x01 /* Mode: 8086/88 / MCS-80/85 */
#define PIC_ICW4_AUTO           0x02 /* EOI: Auto / Normal */
#define PIC_ICW4_BUF_SLAVE      0x08 /* Buffered mode/slave */
#define PIC_ICW4_BUF_MASTER     0x0c /* Buffered mode/master */
#define PIC_ICW4_SFNM           0x10 /* Special fully nested / Not fully n. */

/* Initializes the PICs. */
void pic_init() {
  pic_remap();

  pic_mask_dev(PIC_TIMER_IRQ);
  pic_mask_dev(PIC_KEYBOARD_IRQ);
  pic_mask_dev(PIC_SERIAL_2_IRQ);
  pic_mask_dev(PIC_SERIAL_1_IRQ);
  pic_mask_dev(PIC_RESERVED_1);
  pic_mask_dev(PIC_FDD_IRQ);
  pic_mask_dev(PIC_LPT1_IRQ);
  pic_mask_dev(PIC_CMOS_RTC_IRQ);
  pic_mask_dev(PIC_CGA_VERT_RET_IRQ);
  pic_mask_dev(PIC_RESERVED_2);
  pic_mask_dev(PIC_RESERVED_3);
  pic_mask_dev(PIC_AUX_DEV_IRQ);
  pic_mask_dev(PIC_FPU_IRQ);
  pic_mask_dev(PIC_PRIMARY_ATA_IRQ);
  pic_mask_dev(PIC_SECONDARY_ATA_IRQ);
}

/* Sends end-of-interrupt to PIC. When a device handler finishes handling the
 * interrupt it must call this function to tell the PIC about it so it clears
 * the interrupt line to the processor. */
void pic_send_eoi(itr_irq_t irq) {
  if (irq >= PIC_MASTER_BASE_IRQ &&
      irq <= PIC_MASTER_BASE_IRQ + 7) {
    outb(PIC_MASTER_CMD_PORT, PIC_EOI);
  }
  else if (irq >= PIC_SLAVE_BASE_IRQ &&
           irq <= PIC_SLAVE_BASE_IRQ + 7) {
    outb(PIC_SLAVE_CMD_PORT, PIC_EOI);
    outb(PIC_MASTER_CMD_PORT, PIC_EOI);
  }
}

/* TODO: Check why I coded this. I'm sure there's a need for this, I just
 *       can't remember which it is. */
void pic_io_wait() {
  /* Since the PICs have no status registers and since we can't use interrupts
   * when we program them, we need to ensure the value has been received
   * in the intended port. However, the CPU is a lot faster, so writing
   * carelessly to the ports may cause malfunctioning. This is a Linux
   * kernel courtesy, thanks to OSDev.org. Port 0x80 is used for checkpoints
   * during POST, and since the Linux kernel thinks it's fine to use it
   * I won't be the one complaining. */
   outb(0x80, 0);
}

void pic_remap() {
  unsigned char master_mask, slave_mask;

  /* Get the current PIC masks. */
  master_mask = inb(PIC_MASTER_DATA_PORT);
  slave_mask = inb(PIC_SLAVE_DATA_PORT);

  /* After the initilization command has been set, the PIC will wait for
   * two or three ICW to be sent to its data port, depending on the value of
   * the ICW1_IWC4 bit in the first word.. */
  outb(PIC_MASTER_CMD_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);
  outb(PIC_SLAVE_CMD_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);
  /* IWC2: base IRQ */
  outb(PIC_MASTER_DATA_PORT, PIC_MASTER_BASE_IRQ);
  outb(PIC_SLAVE_DATA_PORT, PIC_SLAVE_BASE_IRQ);
  /* IWC3: master-slave config. */
  outb(PIC_MASTER_DATA_PORT, 4);  /* Tell the master there's a slave at IRQ2 */
  outb(PIC_SLAVE_DATA_PORT, 2);   /* Tell the slave it's IRQ */
  /* ICW 4: FLAGS */
  outb(PIC_MASTER_DATA_PORT, PIC_ICW4_8086); /* Tell them to use 8086 mode */
  outb(PIC_SLAVE_DATA_PORT, PIC_ICW4_8086);

  /* Reset the masks. */
  outb(PIC_MASTER_DATA_PORT, master_mask);
  outb(PIC_SLAVE_DATA_PORT, slave_mask);
}


void pic_mask_dev(enum pic_dev dev) {
  unsigned char mask;

  if (dev >= PIC_MASTER_BASE_IRQ && dev <= PIC_MASTER_BASE_IRQ + 7) {
    mask = inb(PIC_MASTER_DATA_PORT);
    mask = mask | (1 << (dev - PIC_MASTER_BASE_IRQ));
    outb(PIC_MASTER_DATA_PORT, mask);
  }
  else if (dev >= PIC_SLAVE_BASE_IRQ && dev <= PIC_SLAVE_BASE_IRQ + 7) {
    mask = inb(PIC_SLAVE_DATA_PORT);
    mask = mask | (1 << (dev - PIC_SLAVE_BASE_IRQ));
    outb(PIC_SLAVE_DATA_PORT, mask);
  }
}

void pic_unmask_dev(enum pic_dev dev) {
  unsigned char mask;

  if (dev >= PIC_MASTER_BASE_IRQ && dev <= PIC_MASTER_BASE_IRQ + 7) {
    mask = inb(PIC_MASTER_DATA_PORT);
    mask = mask & ~(1 << (dev - PIC_MASTER_BASE_IRQ));
    outb(PIC_MASTER_DATA_PORT, mask);
  }
  else if (dev >= PIC_SLAVE_BASE_IRQ && dev <= PIC_SLAVE_BASE_IRQ + 7) {
    mask = inb(PIC_SLAVE_DATA_PORT);
    mask = mask & ~(1 << (dev - PIC_SLAVE_BASE_IRQ));
    outb(PIC_SLAVE_DATA_PORT, mask);
  }
}
