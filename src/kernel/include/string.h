#ifndef __STRING_H__
#define __STRING_H__

#include <typedef.h>

u32 strlen(char *s);

int strcmp(char *s, char *z);

void strcpy(char *dst, char *src);

char * strtok(char *str, char delim);

void itoa(u32 i, u8 base, char *buf, u8 padding);

void memset(void *mem, u8 value, u32 count);

void memcpy(void *dst, void *src, u32 count);

int memcmp(void *p1, void *p2, u32 count);

int sprintf(char *dst, char *format, ...);

#endif
