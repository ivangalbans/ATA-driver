#include <typedef.h>
#include <io.h>
#include <string.h>
#include <serial.h>
#include <interrupts.h>
#include <mem.h>
#include <hw.h>
#include <pic.h>

/* UART is the chipset implementing the serial port. These are it's registers
 * expressed in terms of offsets relative to the base chipset base address.
 *
 * UART Registers
 * Base Address  DLAB  I/O Access  Abbrv.  Register Name
 *     +0         0    Write       THR     Transmitter Holding Buffer
 *     +0         0    Read        RBR     Receiver Buffer
 *     +0         1    Read/Write  DLL     Divisor Latch Low Byte
 *     +1         0    Read/Write  IER     Interrupt Enable Register
 *     +1         1    Read/Write  DLH     Divisor Latch High Byte
 *     +2         x    Read        IIR     Interrupt Identification Register
 *     +2         x    Write       FCR     FIFO Control Register
 *     +3         x    Read/Write  LCR     Line Control Register
 *     +4         x    Read/Write  MCR     Modem Control Register
 *     +5         x    Read        LSR     Line Status Register
 *     +6         x    Read        MSR     Modem Status Register
 *     +7         x    Read/Write  SR      Scratch Register
 */

/* Serial ports' offsets. base is the base address of a COM device. */
#define SERIAL_DATA_PORT(base)                    (base)
#define SERIAL_ENABLED_INTERRUPTS_PORT(base)      ((base) + 1)
#define SERIAL_FIFO_INTERRUPT_PORT(base)          ((base) + 2)
#define SERIAL_FIFO_CONTROL_PORT(base)            ((base) + 2)
#define SERIAL_LINE_CONTROL_PORT(base)            ((base) + 3)
#define SERIAL_MODEM_CONTROL_PORT(base)           ((base) + 4)
#define SERIAL_LINE_STATUS_PORT(base)             ((base) + 5)
#define SERIAL_MODEM_STATUS_PORT(base)            ((base) + 6)
#define SERIAL_SCRATCH_PORT(base)                 ((base) + 7)

#define SERIAL_DEVICE_INVALID                     0

/* When DLAB is enabled DATA and INTERRUPT_ENABLE become DIVISOR_LSB and
 * DIVISOR_MSB respectively. */
#define SERIAL_DIVISOR_LSB_PORT(base)             (base)
#define SERIAL_DIVISOR_MSB_PORT(base)             ((base) + 1)

/* DLAB, which switches the interpretation on ports offsets 0 and 1, is the
 * highest bit in LINE_CONTROL port. */
#define SERIAL_ENABLE_DLAB                        0x80

/* FIFO Control Register-related flags */
#define SERIAL_FIFO_CTRL_ENABLE_FIFO              0x01
#define SERIAL_FIFO_CTRL_CLEAR_RECV_FIFO          0x02
#define SERIAL_FIFO_CTRL_CLEAR_TRSM_FIFO          0x04
#define SERIAL_FIFO_CTRL_DMA_MODE_SELECT          0x08
#define SERIAL_FIFO_CTRL_ENABLE_64_bytes_FIFO     0x20
#define SERIAL_FIFO_CTRL_INT_LEVEL_1              0x00
#define SERIAL_FIFO_CTRL_INT_LEVEL_4_16           0x40
#define SERIAL_FIFO_CTRL_INT_LEVEL_8_32           0x80
#define SERIAL_FIFO_CTRL_INT_LEVEL_14_56          0xc0

/* Status bits for LINE protocol. */
#define SERIAL_LINE_STATUS_DATA_RECEIVED          0x01
#define SERIAL_LINE_STATUS_OVERRUN_ERROR          0x02
#define SERIAL_LINE_STATUS_PARITY_ERROR           0x04
#define SERIAL_LINE_STATUS_FRAMING_ERROR          0x08
#define SERIAL_LINE_STATUS_BREAK_INTERRUPT        0x10
#define SERIAL_LINE_STATUS_EMPTY_TRANSMITTER_REG  0x20
#define SERIAL_LINE_STATUS_EMPTY_DATA_HOLDING_REG 0x40
#define SERIAL_LINE_STATUS_ERROR_IN_RECV_FIFO     0x20

typedef struct serial_buffer {
  u32 write_head;
  u32 read_head;
  char *buffer;
} serial_buffer_t;

#define SERIAL_TOTAL_DEVICES                       4
#define SERIAL_BUFFER_LEN                          64
#define SERIAL_OFFSET_COM1                         0
#define SERIAL_OFFSET_COM2                         1
#define SERIAL_OFFSET_COM3                         2
#define SERIAL_OFFSET_COM4                         3
#define SERIAL_OFFSET_INVALID                     -1

serial_buffer_t serial_buffers[SERIAL_TOTAL_DEVICES];

int serial_dev2offset(serial_device_t dev);
int serial_irq2offset(itr_irq_t irq);
serial_device_t serial_irq2dev(itr_irq_t irq);
void serial_interrupt_handler(itr_cpu_regs_t regs,
                              itr_intr_data_t data,
                              itr_stack_state_t stack);


int serial_init(serial_device_t dev, serial_port_config_t *config) {
  int serial_offset;
  serial_buffer_t *buffer;

  serial_offset = serial_dev2offset(dev);
  if (serial_offset == SERIAL_OFFSET_INVALID) {
    return -1;
  }
  buffer = serial_buffers + serial_offset;

  /* Set divisor. */
  outb(SERIAL_LINE_CONTROL_PORT(dev), SERIAL_ENABLE_DLAB);
  outb(SERIAL_DIVISOR_LSB_PORT(dev), (u8)(config->divisor & 0x00ff));
  outb(SERIAL_DIVISOR_MSB_PORT(dev), (u8)((config->divisor >> 8) & 0x00ff));

  /* Set line control register while clearing DLAB, just in case. */
  outb(SERIAL_LINE_CONTROL_PORT(dev), config->line_config & 0x7f);

  /* Set interrupts. */
  outb(SERIAL_ENABLED_INTERRUPTS_PORT(dev), config->available_interrupts);

  outb(SERIAL_FIFO_CONTROL_PORT(dev), SERIAL_FIFO_CTRL_ENABLE_FIFO |
                                      SERIAL_FIFO_CTRL_CLEAR_TRSM_FIFO |
                                      SERIAL_FIFO_CTRL_CLEAR_RECV_FIFO |
                                      SERIAL_FIFO_CTRL_INT_LEVEL_1);

  /* Set IRQ for modem. TODO: Check why 0x0b. */
  outb(SERIAL_MODEM_CONTROL_PORT(dev), 0x0);

  /* Prepare the buffer for the device. */
  buffer->buffer = (char *)kalloc(SERIAL_BUFFER_LEN);
  if (buffer->buffer == NULL) {
    return -1;
  }
  buffer->write_head = 0;
  buffer->read_head = 0;

  itr_set_interrupt_handler(PIC_SERIAL_1_IRQ,
                            serial_interrupt_handler,
                            IDT_PRESENT | IDT_DPL_RING_0 | IDT_GATE_INTR);

  return 0;
}

void serial_write(serial_device_t dev, void *buf, u32 len){
  int i;
  for (i = 0; i < len; i++) {
    while ((inb(SERIAL_LINE_STATUS_PORT(dev)) &
            SERIAL_LINE_STATUS_EMPTY_TRANSMITTER_REG) == 0);
    outb(SERIAL_DATA_PORT(dev), ((u8*)(buf))[i]);
  }
}

u32 serial_read(serial_device_t dev, void *buf, u32 len) {
  serial_buffer_t *buffer;
  int offset;

  if (len < 1)
    return 0;

  offset = serial_dev2offset(dev);
  if (offset == SERIAL_OFFSET_INVALID) {
    return 0;
  }
  buffer = serial_buffers + offset;
  while (buffer->write_head == buffer->read_head)
    hw_hlt();
  /* Read just one byte. */
  ((char *)buf)[0] = buffer->buffer[buffer->read_head];
  buffer->read_head = (buffer->read_head + 1) % SERIAL_BUFFER_LEN;
  return 1;
}

int serial_dev2offset(serial_device_t dev) {
  switch (dev) {
    case SERIAL_COM1:
      return SERIAL_OFFSET_COM1;
    case SERIAL_COM2:
      return SERIAL_OFFSET_COM2;
    case SERIAL_COM3:
      return SERIAL_OFFSET_COM3;
    case SERIAL_COM4:
      return SERIAL_OFFSET_COM4;
    default:
      return SERIAL_OFFSET_INVALID;
  }
}

int serial_irq2offset(itr_irq_t irq) {
  switch (irq) {
    case PIC_SERIAL_1_IRQ:
      return SERIAL_OFFSET_COM1;
    case PIC_SERIAL_2_IRQ:
      return SERIAL_OFFSET_COM2;
    /* Which are COM3's and COM4's IRQs? */
    default:
      return SERIAL_OFFSET_INVALID;
  }
}

serial_device_t serial_irq2dev(itr_irq_t irq) {
  switch (irq) {
    case PIC_SERIAL_1_IRQ:
      return SERIAL_COM1;
    case PIC_SERIAL_2_IRQ:
      return SERIAL_COM2;
    /* Which are COM3's and COM4's IRQs? */
    default:
      return SERIAL_DEVICE_INVALID;
  }
}

void serial_interrupt_handler(itr_cpu_regs_t regs,
                              itr_intr_data_t data,
                              itr_stack_state_t stack) {
  serial_buffer_t *buffer;
  int offset;
  serial_device_t dev;

  offset = serial_irq2offset(data.irq);
  if (offset != SERIAL_OFFSET_INVALID) {
    buffer = serial_buffers + offset;
    dev = serial_irq2dev(data.irq);
    if (dev != SERIAL_DEVICE_INVALID) {
      if (inb(SERIAL_LINE_STATUS_PORT(dev)) &
          SERIAL_LINE_STATUS_DATA_RECEIVED) {
        buffer->buffer[buffer->write_head] = inb(SERIAL_DATA_PORT(dev));
        buffer->write_head = (buffer->write_head + 1) % SERIAL_BUFFER_LEN;
      }
    }
  }
  pic_send_eoi(data.irq);
}
