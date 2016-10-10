/*

This is the header file for the VGA driver. This driver is meant to handle
a VGA adapter in its default, 80x25, text, color, odd/even mode.

*/

#ifndef __FB_H__
#define __FB_H__

/* Colors */
enum fb_vga_color {
  FB_COLOR_BLACK = 0,
  FB_COLOR_BLUE = 1,
  FB_COLOR_GREEN = 2,
  FB_COLOR_CYAN = 3,
  FB_COLOR_RED = 4,
  FB_COLOR_MAGENTA = 5,
  FB_COLOR_BROWN = 6,
  FB_COLOR_LIGHT_GREY = 7,
  FB_COLOR_DARK_GREY = 8,
  FB_COLOR_LIGHT_BLUE = 9,
  FB_COLOR_LIGHT_GREEN = 10,
  FB_COLOR_LIGHT_CYAN = 11,
  FB_COLOR_LIGHT_RED = 12,
  FB_COLOR_LIGHT_MAGENTA = 13,
  FB_COLOR_LIGHT_BROWN = 14,
  FB_COLOR_WHITE = 15,
};

/* FB dimensions. */
#define FB_COLS 80
#define FB_ROWS 25

/* Type definitions */
typedef unsigned char  fb_color_t;
typedef unsigned char  fb_row_t;
typedef unsigned char  fb_col_t;

/* Resets the device to a default state. */
void fb_reset();

/* Foreground color. */
fb_color_t fb_fg_color();
void fb_set_fg_color(fb_color_t);

/* Background color. */
fb_color_t fb_bg_color();
void fb_set_bg_color(fb_color_t);

/* Current position. */
fb_row_t fb_row();
fb_col_t fb_col();
void fb_set_pos(fb_row_t, fb_col_t);

/* Current cursor position. */
fb_row_t fb_cur_row();
fb_col_t fb_cur_col();
void fb_set_cur(fb_row_t, fb_col_t);

/* Writes str at the current position using current color as the color. */
void fb_write(char *str);

/* Clears the screen by writting white spaces all over it. Sets the cursor
 * at the top-left corner. */
void fb_clear();

/* Prints a format string to the framebuffer device. It uses the same format
 * sprintf() uses - internally it calls sprintf. */
int fb_printf(char *fmt, ...);

#endif
