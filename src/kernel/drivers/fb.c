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

void fb_sync_cur() {
  fb_set_cur(fb_row(), fb_col());
}

void fb_write(char *str, u32 len) {
  int i;

  for (i = 0;
       i < len && str[i] != '\0';
       fb_base_addr[pos * 2] = str[i],
       fb_base_addr[pos * 2 + 1] = col,
       pos = (pos + 1) % (FB_COLS * FB_ROWS),
       i ++) {
  }
}

void fb_clear() {
  for (pos = 0; pos < FB_COLS * FB_ROWS; pos ++) {
    fb_base_addr[pos * 2] = ' ';
    fb_base_addr[pos * 2 + 1] = col;
  }
  fb_set_pos(0, 0);
  fb_set_cur(0, 0);
}

int fb_printf(char *fmt, ...) {
  #define STATE_LITERAL       0
  #define STATE_PLACEHOLDER   1
  #define NUM_BUF_LEN        65

  va_list v;
  u32 state, count;
  u8 base;
  char buf[NUM_BUF_LEN];

  char *s;
  u8  b;
  u16 w;
  u32 d;
  u64 q;

  state = STATE_LITERAL;
  va_start(v, fmt);

  for (state = STATE_LITERAL, count = 0; *fmt != '\0'; fmt ++) {
    if (state == STATE_LITERAL) {
      switch (*fmt) {
        case '%':
          state = STATE_PLACEHOLDER;
          break;
        case '\n':
          fb_set_pos((fb_row() + 1) % FB_ROWS, 0);
          count ++;
          break;
        case '\r':
          fb_set_pos(fb_row(), 0);
          count ++;
          break;
        default:
          fb_write(fmt, 1);
          count ++;
      }
    }
    else {
      switch (*fmt) {
        case 's':
          s = va_arg(v, char *);
          break;
        case '%':
          buf[0] = '%';
          buf[1] = '\0';
          s = buf;
          break;
        case 'q':
          q = va_arg(v, u64);
          fmt ++;
          switch (*fmt) {
            case 'b':
              itoa((u32)(q >> 32), 2, buf, 0);
              d = strlen(buf);
              itoa((u32)q, 2, buf + d, 32);
              break;
            case 'o':
            case 'd':
            case 'x':
              itoa((u32)(q >> 32), 16, buf, 0);
              d = strlen(buf);
              itoa((u32)q, 16, buf + d, 8);
              strcpy(buf + d, buf + d + 2);
              break;
            default:
              fb_sync_cur();
              return -1; /* Bad format. */
          }
          s = buf;
          break;
        default:
          switch (*fmt) {
            case 'b':
              b = (u8)(va_arg(v, u32));
              d = (u32)b;
              break;
            case 'w':
              w = (u16)va_arg(v, u32);
              d = (u32)w;
              break;
            case 'd':
              d = va_arg(v, u32);
              break;
            default:
              fb_sync_cur();
              return -1; /* Bad format. */
          }
          fmt++;
          switch (*fmt) {
            case 'b':
              base = 2;
              break;
            case 'o':
              base = 8;
              break;
            case 'd':
              base = 10;
              break;
            case 'x':
              base = 16;
              break;
            default:
              fb_sync_cur();
              return -1; /* Bad format. */
          }
          itoa(d, base, buf, 0);
          s = buf;
          break;
      }
      fb_write(buf, strlen(buf));
      count += strlen(buf);
      state = STATE_LITERAL;
    }
  }
  fb_sync_cur();
  return count;
}
