#ifndef __STRING_H__
#define __STRING_H__

#include "../include/typedef.h"

u32 strlen(char *s);

int strcmp(char *s, char *z);

void strcpy(char *dst, char *src);

char * strtok(char *str, char delim);

void itoh(u32 i, char *buff);

void ltoh(u64 l, char *buff);

void memset(void *mem, u8 value, u32 count);

void memcpy(void *dst, void *src, u32 count);

int memcmp(void *p1, void *p2, u32 count);

#endif
