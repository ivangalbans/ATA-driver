#include <string.h>

static char map[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                        'a', 'b', 'c', 'd', 'e', 'f' };

u32 strlen(char *s) {
  int c;
  for (c = 0; s[c] != 0; c++);
  return c;
}

void itoh(u32 i, char *buff) {
  int c;
  u32 r;
  char tmp[11];
  c = 0;
  while (1) {
    r = i % 16;
    tmp[c] = map[r];
    i = i / 16;
    if (i == 0) break;
    c++;
  }
  while (c < 7) {
    tmp[++c] = '0';
  }
  tmp[++c] = 'x';
  tmp[++c] = '0';
  r = 0;
  while (c >= 0) {
    buff[r] = tmp[c];
    c--;
    r++;
  }
  buff[r] = '\0';
}

void ltoh(u64 l, char *buff) {
  int c;
  u64 r;
  char tmp[19];
  c = 0;
  while (1) {
    r = l % 16;
    tmp[c] = map[r];
    l = l / 16;
    if (l == 0) break;
    c++;
  }
  tmp[++c] = 'x';
  tmp[++c] = '0';
  r = 0;
  while (c >= 0) {
    buff[r] = tmp[c];
    c--;
    r++;
  }
  buff[r] = '\0';
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
