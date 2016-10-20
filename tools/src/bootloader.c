#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btool.h"

struct buhos_boot {
  unsigned short _jmp;
  unsigned int vbr_offset;
  unsigned char _pad[1018];
} __attribute__((__packed__));

/* This subcommands syntax is:
 *    bootloader DISK_IMAGE BOOTLOADER_IMAGE PARTITION_NO
 */
#define HELP_OPT      argv[2]

#define DISK_IMG      argv[2]
#define BOOT_IMG      argv[3]
#define PARTNO        argv[4]

void bootloader_help(int argc, char *argv[]) {
  printf(
"usage: %s %s DISK_IMAGE BOOT_IMAGE [ PARTNO ]\n"
"       DISK_IMAGE : path to the full disk image file.\n"
"       BOOT_IMAGE : path to the base bootloader image to install at PARTNO.\n"
"       PARTNO     : The partition to install the bootloader at. If not\n"
"                    present, the active partition is selected.\n"
  , CMD, HELP_OPT);
}

int bootloader_write(int argc, char *argv[]) {
  FILE *fd, *fb;
  struct buhos_boot boot;
  struct mbr mbr;
  size_t br, bw;
  unsigned int partno;

  if (argc < 4) {
    bootloader_help(argc, argv);
    return 1;
  }
  if (strcmp(HELP_OPT, "--help") == 0) {
    bootloader_help(argc, argv);
    return 0;
  }

  fd = fopen(DISK_IMG, "r+");
  if (fd == NULL)
    scream_and_quit("Could not open disk image.");

  if (fread(&mbr, 1, sizeof(struct mbr), fd) != sizeof(struct mbr))
    scream_and_quit("Could not read mbr.");

  if (argc > 4) {
    partno = atoi(PARTNO);
    if (partno < 0 || partno > 3)
      scream_and_quit("Partition number must be in [0,3].");
  }
  else {
    for (partno = 0; partno < 4; partno ++) {
      if (mbr.entries[partno].status & 0x80)
        break;
    }
    if (partno == 4)
      scream_and_quit("No bootbable partition found.");
  }

  if (mbr.entries[partno].sectors_count == 0)
    scream_and_quit("Selected partition seems to be blank.");

  if (fseek(fd, mbr.entries[partno].lba_start * SECTOR_SIZE, SEEK_SET) == -1)
    scream_and_quit("Could not seek to partition start into disk image.");

  fb = fopen(BOOT_IMG, "r");
  if (fb == NULL)
    scream_and_quit("Could not open bootloader.");
  br = fread(&boot, 1, sizeof(struct buhos_boot), fb);
  if (br < sizeof(struct buhos_boot) && ferror(fb))
    scream_and_quit("Could not read bootloader.");
  fclose(fb);

  boot.vbr_offset = mbr.entries[partno].lba_start;

  bw = fwrite(&boot, 1, br, fd);
  if (bw < br)
    scream_and_quit("Could not write bootloader to disk image.");
  fclose(fd);

  return 0;
}
