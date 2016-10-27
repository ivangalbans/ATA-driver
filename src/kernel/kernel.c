#include <fb.h>
#include <hw.h>
#include <string.h>
#include <mem.h>
#include <pic.h>
#include <serial.h>
#include <kb.h>
#include <ata.h>

/* Just the declaration of the second, main kernel routine. */
void kmain2();

/* In case we run into PANIC. */
void kernel_panic(char *msg) {
  fb_set_fg_color(FB_COLOR_WHITE);
  fb_set_bg_color(FB_COLOR_RED);
  fb_clear();
  fb_write(msg, strlen(msg));
  hw_cli();
  hw_hlt();
  /* And the world stops ... */
}

void kmain(void *gdt_base, void *mem_map) {
  /* This is a hack. We need to set the stack to the right place, but we need
   * to do this AFTER the memory allocator has checked and initialized the
   * memory. To do this we'll manipulate the stack in a dangerous way, but
   * since we won't return from this function its somehow safe. Also, to keep
   * things simple, this function should have no activation registry - or at
   * least that's what I think, since I can't be sure what will GCC do.
   * In any case, after relocating the stack we'll just call kmain_stage_2
   * so there should be no need for anything in the stack since the address
   * to kmain2 must be statically computed. */

  /* Luckily, framebuffer driver is basically static, except for fb_printf.
   * If we have to print anything here let it feel TERRIBLE. */
  fb_reset();

  /* Initialize the memory. We're using the stack set during the real-mode
   * initial steps. */
  if (mem_init(gdt_base, mem_map) == -1) {
    kernel_panic("Could not initialize memory :(");
  }
  /* Our stack will be 4K long situated at the end of the kernel space, right
   * before the user space. We need to allocate this very frame. */
  if (mem_allocate_frames(1,
                          MEM_KERNEL_STACK_FRAME,
                          MEM_USER_FIRST_FRAME) == NULL) {
    kernel_panic("Could not allocate a frame for the kernel's stack :(");
  }

  /* And now comes the magic. Fingers crossed. */
  mem_relocate_stack_to((void *)MEM_KERNEL_STACK_TOP);

  /* At this point, we are standing in the new stack. We can continue but we
   * can't return here. */
  kmain2();
}

void kmain2() {
  serial_port_config_t sc;
  char buf[2];

  fb_reset();
  fb_set_fg_color(FB_COLOR_BLUE);
  fb_set_bg_color(FB_COLOR_WHITE);
  fb_clear();

  /* Initializes the interrupt subsytem. */
  if (itr_set_up() == -1) {
    kernel_panic("Coult not initialize IDT :(");
  }

  /* Initializes the PICs. This mask all interrupts. */
  pic_init();

  /* Activate the keyboard. */
  if (kb_init() == -1) {
    kernel_panic("Could not initialize the keyboard :(");
  }
  pic_unmask_dev(PIC_KEYBOARD_IRQ);

  sc.divisor = 3;  /* Baud rate = 115200 / 3 = 38400 */
  sc.available_interrupts = SERIAL_INT_DATA_AVAILABLE;    /* No interrupts */
  sc.line_config = SERIAL_CHARACTER_LENGTH_8 |  /* Standard 8N1 config */
                   SERIAL_PARITY_NONE |
                   SERIAL_SINGLE_STOP_BIT;
  if (serial_init(SERIAL_COM1, &sc) == -1) {
    kernel_panic("Could not intialize COM1 :(");
  }
  pic_unmask_dev(PIC_SERIAL_1_IRQ);

  /* We can now turn interrupts on, they won't reach us (yet). */
  hw_sti();

  ata_dev_t disk[4]; 
  ata_dev_t *dp[4] = { disk, disk + 1, disk + 2, disk + 3};

  ata_init(dp);

  /* This is the idle loop. */
  while (1) {
    buf[0] = 0; buf[1] = 0;
    serial_read(SERIAL_COM1, buf, 1);
    fb_write(buf, 1);
  }
}
