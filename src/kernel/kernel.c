void print(char *msg) {
  int i;
  char *b = (char *)0xb8000;

  for (i = 0; msg[i] != 0; i++)
    b[i * 2] = msg[i];
}

void kmain(void *gdt_base, void *mem_map) {
  print("hola");
}
