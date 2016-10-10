/* Common type definitions. */

#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

typedef unsigned char            u8;
typedef unsigned short          u16;
typedef unsigned int            u32;
typedef unsigned long long  int u64;

typedef char                     s8;
typedef short                   s16;
typedef int                     s32;
typedef long long int           s64;

#define TRUE                      1
#define FALSE                     0

#define NULL             (void*)0x0

typedef __builtin_va_list        va_list;
#define va_start(v, l)            __builtin_va_start(v, l)
#define va_end(v)                 __builtin_va_end(v)
#define va_arg(v, l)              __builtin_va_arg(v, l)

#endif
