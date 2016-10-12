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

/* Initialization commands (Initialization Command Word N) bits. Check
 * pic_remap below for an explanation of how it works. */

/*****************************************************************************
 MACRO                          Bit     Meaning
============================================================================= */
#define PIC_ICW1_INIT           0x10 /* Initialization command. This one is
                                      * required to start the sequence. */
#define PIC_ICW1_ICW4           0x01 /* ICW4 needed. If set, the initialization
                                      * sequence will be four ICW long,
                                      * otherwise it'll be just three. */
#define PIC_ICW1_SINGLE         0x02 /* Operation mode. If set the PIC will
                                      * operate in single mode, which means
                                      * the master/slave set up won't work.
                                      * If not set the PICs will work in
                                      * cascaded (master/slave) mode. */
#define PIC_ICW1_INTERVAL4      0x04 /* Call address interval. If set the
                                      * interval is 4, otherwise it's 8.
                                      * TODO: What's this? */
#define PIC_ICW1_LEVEL          0x08 /* Trigger mode. If set, the PIC works
                                      * level triggered, otherwise it's edge
                                      * triggered. */

#define PIC_ICW4_8086           0x01 /* Mode. If set the PIC works in 8086/88
                                      * mode, otherwise in MCS-80/85. Remember
                                      * this is a very old device :). */
#define PIC_ICW4_AUTO           0x02 /* EOI mode. If set it's auto, otherwise
                                      * it's normal.
                                      * TODO: I guess auto means sending EOI
                                      * automatically to the device and normal
                                      * means to wait for an EOI command to be
                                      * issued from the processor, but I must
                                      * check this. */
#define PIC_ICW4_BUF_SLAVE      0x08 /* Buffered mode/slave. TODO: WTF? */
#define PIC_ICW4_BUF_MASTER     0x0c /* Buffered mode/master. TODO: WTF? */
#define PIC_ICW4_SFNM           0x10 /* Special fully nested / Not fully n.
                                      * TODO: WTF? */

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

  /* To initialize the 8259A PIC three or four Initialization Command Words
   * (ICW) must be provided.
   *  1. The sequence starts once PIC_ICW1_INIT is written to the PIC's command
   *     port, and that's the first ICW. With it comes some flags, all defined
   *     as PIC_ICW1_*, of which PIC_ICW1_ICW4, if set, tells the PIC the
   *     sequence will be four commands long, otherwise it'll be only three.
   *     The remaining ICW are written into the data port, not the command one.
   *  2. The second ICW sets the base vector offset used by this PIC. This
   *     vector offset is relative to the IDT, so it must be synchronized with
   *     the registered interrupt handlers set up in the IDT. Let's say the
   *     base vector offset is 0x20, then when the keyboard sends an interrupt
   *     using its reserved IRQ 1 (which can not be changed) the PIC gets the
   *     last three bits of the IRQ and adds it to the base vector offset, thus
   *     interrupting the processor with IRQ 0x21 which in turn will make the
   *     hardware call the handler set at IDT[0x21]. Keep in mind this base
   *     vector offset must be aligned to values of 8, i.e. the last three
   *     bits must be 0 because what actually happens is not a real sum but
   *     just replacing the last three bits in the offset by the original IRQ.
   *  3. The third word configures the mastes/slave set-up, at least in
   *     cascaded mode, by telling the master what's the slave's IRQ and the
   *     slave what's its own IRQ.
   *     TODO: What if in single mode? My guess is this is the word that will
   *     be skipped if ICW1_IWC4 is not set, because the last one has more
   *     meaningful configuration bits. However, I might be wrong and that
   *     fourth word is for advanced features (in the time it was first
   *     conceived) and this third one tells each PIC it's own IRQ.
   *  4. Last word, if present, sets several operation modes, including the
   *     processor family (it seems there were two families at the time using
   *     this same chip) and some buffering options I don't understand yet.
   */

  /* Get the current PIC masks. */
  master_mask = inb(PIC_MASTER_DATA_PORT);
  slave_mask = inb(PIC_SLAVE_DATA_PORT);

  /* After the initilization command has been set, the PIC will wait for
   * two or three ICW to be sent to its data port, depending on the value of
   * the ICW1_IWC4 bit in the first word. */
  outb(PIC_MASTER_CMD_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);
  outb(PIC_SLAVE_CMD_PORT, PIC_ICW1_INIT | PIC_ICW1_ICW4);

  /* IWC2: base IRQ */
  outb(PIC_MASTER_DATA_PORT, PIC_MASTER_BASE_IRQ);
  outb(PIC_SLAVE_DATA_PORT, PIC_SLAVE_BASE_IRQ);

  /* IWC3: master-slave config. */
  outb(PIC_MASTER_DATA_PORT, 0x04); /* Tell the master the slave's PIC IRQ is
                                     * in line 2 (00000100) = 0x04. */
  outb(PIC_SLAVE_DATA_PORT, 0x02);  /* Tell the slave it's IRQ */

  /* ICW 4: FLAGS */
  outb(PIC_MASTER_DATA_PORT, PIC_ICW4_8086); /* Tell them to use 8086 mode */
  outb(PIC_SLAVE_DATA_PORT, PIC_ICW4_8086);

  /* Reset the masks. */
  outb(PIC_MASTER_DATA_PORT, master_mask);
  outb(PIC_SLAVE_DATA_PORT, slave_mask);
}

/* Masking an interrupt means telling the PIC to "silence" it (i.e. never
 * interrupting the processor when this interrupt arrives). */
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

/* Unmasking an interrupt is the oposite to masking, isn't it? */
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
