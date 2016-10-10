#include <string.h>

void print(char *msg) {
  int i;
  char *b = (char *)0xb8000;

  for (i = 0; msg[i] != 0; i++)
    b[i * 2] = msg[i];
}

void kmain(void *gdt_base, void *mem_map) {
  print("lola");

  char *msg = "Hola %dd %wo %bb %qx";
  char buf[100];
  sprintf(buf, msg, 1000, 0724, 0xf4, 0xa0b1c2d3e4f567);
  print(buf);
}
