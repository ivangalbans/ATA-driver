/* This is the VGA's Framebuffer driver. */

#include <typedef.h>
#include <io.h>
#include <fb.h>
#include <string.h>

typedef unsigned char  fb_packed_color_t;
typedef unsigned short fb_coord_t;

#define VGA_CRT_ADDR_PORT                     0x03d4
#define VGA_CRT_DATA_PORT                     0x03d5
#define VGA_CRT_CURSOR_LOCATION_HIGH_REGISTER 0x0e
#define VGA_CRT_CURSOR_LOCATION_LOW_REGISTER  0x0f

/* Combines background and foreground colors into a byte suitable as an
 * attribute to put in plane 1. */
#define FB_PACK_COLORS(fg,bg) (fb_packed_color_t)(((fb_color_t)(fg) & 0x0f) | ((fb_color_t)(bg) << 4))
/* Reverse the operation above. */
#define FB_BG_COLOR(pc) (fb_color_t)((fb_packed_color_t)(pc) >> 4)
#define FB_FG_COLOR(pc) (fb_color_t)((fb_packed_color_t)(pc) & 0x0f)

/* Combines a pair of row and column into a single packed short used in some operations. */
#define FB_COORDS(r, c) (fb_coord_t)((fb_row_t)(r) * FB_COLS + (fb_col_t)(c))
/* Reverse the operation above. */
#define FB_ROW(coord) (fb_row_t)((fb_coord_t)(coord) / FB_COLS)
#define FB_COL(coord) (fb_col_t)((fb_coord_t)(coord) % FB_COLS)

/* State variables. */
static fb_coord_t pos;
static fb_coord_t cur;
static fb_packed_color_t col;

/* Framebuffer base address */
static char *fb_base_addr = (char*)0xb8000;

void fb_reset() {
  pos = FB_COORDS(0, 0);
  cur = FB_COORDS(0, 0);
  col = FB_PACK_COLORS(FB_COLOR_WHITE, FB_COLOR_BLACK);
}

fb_color_t fb_fg_color() {
  return FB_FG_COLOR(col);
}

void fb_set_fg_color(fb_color_t fg) {
  if (fg > FB_COLOR_WHITE)
    return;
  fb_color_t bg = FB_BG_COLOR(col);
  col = FB_PACK_COLORS(fg, bg);
}

fb_color_t fb_bg_color() {
  return FB_BG_COLOR(col);
}

void fb_set_bg_color(fb_color_t bg) {
  if (bg > FB_COLOR_WHITE)
    return;
  fb_color_t fg = FB_FG_COLOR(col);
  col = FB_PACK_COLORS(fg, bg);
}


/* pos */

fb_row_t fb_row() {
  return FB_ROW(pos);
}

fb_col_t fb_col() {
  return FB_COL(pos);
}

void fb_set_pos(fb_row_t r, fb_col_t c) {
  if (r < FB_ROWS && c < FB_COLS)
    pos = FB_COORDS(r, c);
}


/* cursor */

fb_row_t fb_cur_row() {
  return FB_ROW(cur);
}

fb_col_t fb_cur_col() {
  return FB_COL(cur);
}

void fb_set_cur(fb_row_t r, fb_col_t c) {
  if (r < FB_ROWS && c < FB_COLS) {
    cur = FB_COORDS(r, c);
    outb(VGA_CRT_ADDR_PORT, VGA_CRT_CURSOR_LOCATION_HIGH_REGISTER);
    outb(VGA_CRT_DATA_PORT, (unsigned char)(cur >> 8));
    outb(VGA_CRT_ADDR_PORT, VGA_CRT_CURSOR_LOCATION_LOW_REGISTER);
    outb(VGA_CRT_DATA_PORT, (unsigned char)(cur));
  }
}

void fb_write(char *str) {
  int count;

  for (count = 0;
       str[count] != '\0';
       fb_base_addr[pos * 2] = str[count],
       fb_base_addr[pos * 2 + 1] = col,
       pos = (pos + 1) % (80  *25),
       count ++) {
  }

  fb_set_cur(FB_ROW(pos), FB_COL(pos));
}

void fb_clear() {
  int i;

  for (i = 0; i < 80 * 25; i++) {
    fb_base_addr[2 * i] = ' ';
    fb_base_addr[2 * i + 1] = col;
  }

  fb_set_cur(0, 0);
}

int fb_printf(char *fmt, ...) {
  return 0;
}
