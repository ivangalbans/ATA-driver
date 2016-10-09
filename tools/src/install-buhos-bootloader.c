#include <stdio.h>
#include <stdlib.h>

struct minix_boot {
  unsigned short _jmp;
  unsigned int vbr_offset;
  unsigned char _pad[1018];
} __attribute__((__packed__));

void scream_and_quit(char *msg) {
  fprintf(stderr, msg);
  exit(1);
}

/* args: disk image, vbr image, sector. */
int main(int argc, char *argv[]) {
  FILE *fd;
  struct minix_boot boot;
  size_t br, bw;
  unsigned int sector;

  printf("Installing bootloader %s on %s at %d\n", argv[2], argv[1], atoi(argv[3])); 

  fd = fopen(argv[2], "r");
  if (!fd)
    scream_and_quit("Could not open minix bootloader.");
  br = fread(&boot, 1, sizeof(struct minix_boot), fd);
  if (br < sizeof(struct minix_boot) && ferror(fd))
    scream_and_quit("Could not read minix bootloader.");
  fclose(fd);

  sector = (unsigned int)(atoi(argv[3]));
  boot.vbr_offset = sector;

  fd = fopen(argv[1], "r+");
  if (!fd)
    scream_and_quit("Could not open disk image.");
  if (fseek(fd, sector * 512, SEEK_SET) == -1)
    scream_and_quit("Could not seek into disk image.");
  bw = fwrite(&boot, 1, br, fd);
  if (bw < br && ferror(fd))
    scream_and_quit("Could not write minix bootloader to disk.");
  fclose(fd);

  return 0;
}
