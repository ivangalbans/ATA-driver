#include <string.h>
#include <math.h>

#define NUM_BUF_LEN   65

u32 strlen(char *s) {
  int c;
  for (c = 0; s[c] != 0; c++);
  return c;
}

/* Transforms a number into a string. Allowed bases are 2, 8, 10 and 16. */
void itoa(u32 i, u8 base, char *buf, u8 padding) {
  s32 c; /* I'd use s8, but -Wchar-subscripts yells at me. */
  u8 r;

  char tmp[NUM_BUF_LEN];
  char map[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                   'a', 'b', 'c', 'd', 'e', 'f' };

  memset(tmp, '\0', NUM_BUF_LEN);

  if (i == 0) {
    c = 0;
    tmp[c] = '0';
    c ++;
  }
  else {
    for (c = 0; i > 0; c++) {
      r = i % base;
      tmp[c] = map[r];
      i = i / base;
    }
  }

  for (; c < padding; tmp[c] = '\0', c ++);

  switch (base) {
    case 2:
    case 10:
      c --;
      break;
    case 8:
      tmp[c] = '0';
      break;
    case 16:
      tmp[c++] = 'x';
      tmp[c] = '0';
      break;
  }

  for (r = 0; c >= 0; r ++, c --)
    buf[r] = tmp[c];

  buf[r] = '\0';
}

void memset(void *mem, u8 value, u32 count) {
  u32 i;
  for (i = 0; i < count; ((char *)mem)[i++] = value);
}

void memcpy(void *dst, void *src, u32 count) {
  u32 i;
  for (i = 0; i < count; ((char *)dst)[i] = ((char *)src)[i], i++);
}

int memcmp(void *p1, void *p2, u32 count) {
  u32 i;
  for (i = 0; i < count; i++) {
    if (((char *)p1)[i] < ((char *)p2)[i])
      return -1;
    if (((char *)p1)[i] > ((char *)p2)[i])
      return 1;
  }
  return 0;
}

int strcmp(char *s, char *z) {
  int i;
  i = 0;

  while (1) {
    if (s[i] < z[i])
      return -1;
    if (s[i] > z[i])
      return 1;
    if (s[i] == 0)
      break;
    i += 1;
  }
  return 0;
}

void strcpy(char *dst, char *src) {
  int i;
  for (i = 0; src[i] != 0; dst[i] = src[i], i++);
  dst[i] = 0;
}

char * strtok(char *str, char delim) {
  static char *ptr = NULL;
  char *p;

  if (str != NULL)
    ptr = str;

  /* Find leading delims. */
  while (*ptr == delim) {
    if (*ptr == 0)
      break;
    ptr += 1;
  }

  if (*ptr == 0)
    return NULL;
  p = ptr;

  /* Find the next delimiter and clear it. */
  while (*ptr != delim) {
    if (*ptr == 0) {
      /* We got the end of the string so we're good to go. */
      break;
    }
    ptr += 1;
  }

  if (*ptr != 0) {
    *ptr = 0;
    ptr += 1;
  }

  return p;
}

int sprintf(char *dst, char *fmt, ...) {
  #define STATE_LITERAL       0
  #define STATE_PLACEHOLDER   1

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
        default:
          *dst = *fmt;
          dst ++;
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
              return -1; /* Bad format. */
          }
          itoa(d, base, buf, 0);
          s = buf;
          break;
      }
      for (; *s != '\0'; *dst = *s, dst ++, s ++, count ++);
      state = STATE_LITERAL;
    }
  }
  return count;
}
