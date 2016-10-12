#include <interrupts.h>
#include <typedef.h>
#include <string.h>
#include <mem.h>
#include <pic.h>
#include <fb.h>

#define IDT_ENTRIES               256

/* IDT entry */
typedef struct {
  u16 off_low;
  u16 segment;
  u16 flags;
  u16 off_high;
} __attribute__((__packed__)) itr_idt_entry_t;

/* IDT */
typedef struct {
  u16 limit;
  itr_idt_entry_t *base_address;
} __attribute__((__packed__)) itr_lidt_t;

/* These are the two globals that will make all this work. */
/* This array will store all interrupt handlers currently active. The index
 * is the IRQ. */
interrupt_handler_t *interrupt_handlers;

/* This is the actual IDT. This needs to be aligned. */
itr_idt_entry_t *idt;

/* This is the generic handler that will be called from assembly code whenever
 * an interrupt is issued. */
void itr_interrupt_handler(itr_cpu_regs_t regs,
                           itr_intr_data_t intr,
                           itr_stack_state_t stack) {
  /* No one should call this except for the assembly code, so there's no need
   * to check intr.irq for correctness. */
  interrupt_handler_t ih = interrupt_handlers[intr.irq];
  if (ih != NULL) {
    /* Call the handler. */
    (*ih)(regs, intr, stack);
  }
  else {
    fb_printf(">> Generic intr handler called with IRQ: %dd\n", intr.irq);
    /* This only works because we know we only deal with the 8259 PICs.
     * That said, let's just silence the interrupt -some day we'll also log
     * it-. */
    pic_send_eoi(intr.irq);
  }
}

/* Set an interrupt handler. This will activate the entry in the IDT and
 * attach the handler. Use flags to set the flags of the gate. */
void itr_set_interrupt_handler(itr_irq_t irq,
                               interrupt_handler_t handler,
                               u16 flags) {
  interrupt_handlers[irq] = handler;
  idt[irq].flags = flags;
}

/* These are set in interrupts.asm. */
extern void itr_set_idt_entries_offsets(itr_idt_entry_t *);
extern void itr_load_idt(itr_lidt_t *);

/* Initialize IDT and the whole interruption system. */
int itr_set_up() {
  int i;
  itr_lidt_t l;

  interrupt_handlers =
    (interrupt_handler_t *)kalloc(sizeof(interrupt_handler_t) * IDT_ENTRIES);
  if (interrupt_handlers == NULL)
    return -1;

  idt = (itr_idt_entry_t *)kalloc(sizeof(itr_idt_entry_t) * IDT_ENTRIES);
  if (idt == NULL)
    return -1;

  /* Clear all interrupt handlers. */
  for (i = 0; i < 256; i++)
    interrupt_handlers[i] = NULL;

  /* Ask the assembly code to fill the offsets. */
  itr_set_idt_entries_offsets(idt);

  /* Set up the rest of the necessary data. This doesn't set any handler:
   * when handlers get registered then the corresponding entries will be
   * set as present. */
  for (i = 0; i < 256; i++) {
    idt[i].segment = GDT_KERNEL_CODE_SEGMENT;
    idt[i].flags = IDT_PRESENT | IDT_GATE_INTR | IDT_DPL_RING_0;
  }

  /* Load IDT. To load the IDT we need a 6 bytes structure: the lower 2 bytes
   * are the limit, the upper four are the base address. We can't naturally
   * pass such struct in the stack, so let's build it up here and pass the
   * the address to it. */
  l.base_address = idt;
  l.limit = IDT_ENTRIES * sizeof(itr_idt_entry_t) - 1;
                              /* I discovered why -1: Intel adds the limit to
                               * the base address to figure out the last valid
                               * address. Then, a value of 0 means there's one
                               * byte, and that applies to everything else. */
  itr_load_idt(&l);

  return 0;
}
