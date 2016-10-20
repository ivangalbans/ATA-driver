#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "btool.h"

void help(int argc, char *argv[]) {
  printf(
"usage: %s COMMAND [--help|CMD_ARGS]\n"
"       COMMAND can be:\n"
"         mbr     : Partitions the disk and installs boot code into MBR.\n"
"         boot    : Installs the bootloader into the given partition.\n"
"         kernel  : Installs the kernel.\n"
"         help : Prints this small help and exits.\n"
  , PROG);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    help(argc, argv);
    exit(0);
  }

  if (strcmp(CMD, "mbr") == 0)
    return mbr_write(argc, argv);
  else if (strcmp(CMD, "boot") == 0)
    return bootloader_write(argc, argv);
  else if (strcmp(CMD, "kernel") == 0)
    return minix_write(argc, argv);
  else
    help(argc, argv);

  return 0;
}

void scream_and_quit(char *msg) {
  fprintf(stderr, msg);
  exit(1);
}
