#include <kb.h>
#include <string.h>
#include <fb.h>
#include <pic.h>
#include <io.h>
#include <interrupts.h>
#include <mem.h>

/* These are the ports managing the keyboard. Many registers are associated to
 * them. However, none of them is read/write, thus the operation itself
 * determines the register in use.
 *
 *  Keyboard Encoder (keyboard's own microcontroller)
 *    0x60    Read    Read Input Buffered
 *    0x60    Write   Send Command
 *  Keyboard Controller (keyboard's controller in the chipset)
 *    0x64    Read    Status registers
 *    0x64    Write   Send Command
 *
 */
#define KB_ENCODER_BUF        0x60
#define KB_ENCODER_CMD        0x60
#define KB_CONTROLLER_STATUS  0x64
#define KB_CONTROLLER_CMD     0x64

#define KB_BREAK              0x0f
#define KB_THIRD_LEVEL        0xe0

/* We'll have a buffer to hold the scan codes. */
#define KB_BUF_LEN            32  /* TODO: Is this right? */
static unsigned char *kb_buffer;
static int kb_buf_head;       /* Points to the first valid scan code. */
static int kb_buf_count;      /* Total scan codes in buf */

int kb_init() {
  kb_buf_head = 0;
  kb_buf_count = 0;
  kb_buffer = (char *)kalloc(KB_BUF_LEN);
  if (kb_buffer == NULL)
    return -1;
  itr_set_interrupt_handler(PIC_KEYBOARD_IRQ, kb_interrupt_handler,
                            IDT_PRESENT | IDT_DPL_RING_0 | IDT_GATE_INTR);
}

/* This is the actual interrupt handler. */
void kb_interrupt_handler(itr_cpu_regs_t regs,
                          itr_intr_data_t intr,
                          itr_stack_state_t stack) {
  static unsigned char partial[6];
  static int len = 0;

  int i;
  partial[len++] = inb(KB_ENCODER_BUF);

  do {
    if (len == 1) {
      if (partial[0] == 0xe0 || partial[0] == 0xe1)
        break;
    }
    else if (len == 2) {
      if (partial[0] == 0xe1 || partial[1] == 0x2a || partial[1] == 0xb7)
        break;
    }
    else if (len == 4) {
      if (partial[0] == 0xe1)
        break;
    }
    if (kb_buf_count + len <= KB_BUF_LEN) {
      i = 0;
      while (i < len) {
        kb_buffer[(kb_buf_head + kb_buf_count) % KB_BUF_LEN] = partial[i];
        i++;
        kb_buf_count++;
      }
    }
    len = 0;
  }
  while (0);

  pic_send_eoi(intr.irq); /* We do this because we're correct people, but the
                           * keyboard actually clears the line when you read
                           * from the encoder's buffer. */
}

int kb_scan_code(char *buf) {
  int size = 0;
  if (kb_buf_count == 0) {
    /* empty */
  }
  /* PAUSE */
  else if (kb_buffer[kb_buf_head] == 0xe1) {
    while (size < 6) {
      buf[size] = kb_buffer[kb_buf_head];
      size++;
      kb_buf_head = (kb_buf_head + 1) % KB_BUF_LEN;
    }
  }
  /* PRNTSCR */
  else if (kb_buffer[kb_buf_head] == 0xe0 &&
           (kb_buffer[(kb_buf_head + 1) % KB_BUF_LEN] == 0x2a ||
            kb_buffer[(kb_buf_head + 1) % KB_BUF_LEN] == 0xb7)) {
    while (size < 4) {
      buf[size] = kb_buffer[kb_buf_head];
      size++;
      kb_buf_head = (kb_buf_head + 1) % KB_BUF_LEN;
    }
  }
  /* Two bytes */
  else if (kb_buffer[kb_buf_head] == 0xe0) {
    while (size < 2) {
      buf[size] = kb_buffer[kb_buf_head];
      size++;
      kb_buf_head = (kb_buf_head + 1) % KB_BUF_LEN;
    }
  }
  else {
    size = 1;
    buf[0] = kb_buffer[kb_buf_head];
    kb_buf_head = (kb_buf_head + 1) % KB_BUF_LEN;
  }
  kb_buf_count -= size;
  return size;
}
