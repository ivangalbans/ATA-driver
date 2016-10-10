#include <string.h>
#include <fb.h>
#include <hw.h>

void kmain(void *gdt_base, void *mem_map) {
  fb_reset();
  fb_set_fg_color(FB_COLOR_BLUE);
  fb_set_bg_color(FB_COLOR_WHITE);
  fb_clear();
  fb_write("Hola mundo");
  hw_hlt();
}
