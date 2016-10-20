/* Writes the partition table to the image file. This is something easily done
 * by sfdisk, but we're dealing with a lot of troubles with sfdisk's versions
 * and the incompatibilities between the input of one version and those from
 * the rest. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btool.h"

/* This subcommands syntax is:
 *    mbr DISK_IMAGE MBR_IMAGE [START,SIZE,TYPE,BOOTABLE ... ]
 */

#define HELP_OPT          argv[2]

#define DISK_IMG          argv[2]
#define MBR_IMG           argv[3]
#define FIRST_PART_INDEX  4

void mbr_help(int argc, char *argv[]) {
  printf(
"usage: %s %s DISK_IMAGE MBR_IMAGE [ PARTITION_DEFINITION ... ]\n"
"       DISK_IMAGE : path to the full disk image file.\n"
"       MBR_IMAGE  : path to the base mbr to install at disk sector 0.\n"
"       PARTITION_DEFINITION : START,SIZE,TYPE,BOOT where\n"
"         START : partition's first sector.\n"
"         SIZE  : partition's size in sectors.\n"
"         TYPE  : partition's type (e.g. 131 Linux, 129 Minix).\n"
"         BOOT  : 1 if partition is bootable/active, 0 otherwise.\n"
  , PROG, CMD);
}

int mbr_write(int argc, char *argv[]) {
  FILE *f;
  struct mbr mbr;
  int i;
  char *token;

  if (argc < 4) {
    mbr_help(argc, argv);
    return 1;
  }
  if (strcmp(HELP_OPT, "--help") == 0) {
    mbr_help(argc, argv);
    return 0;
  }

  memset(&mbr, 0, sizeof(struct mbr));

  f = fopen(MBR_IMG, "r");
  if (f == NULL)
    scream_and_quit("Could not open mbr image file.");
  if (fread(&mbr, 1, sizeof(struct mbr), f) < sizeof(struct mbr) && ferror(f))
    scream_and_quit("Could not read mbr image file.");
  fclose(f);

  for (i = 0; i < 4; i ++) {
    if (i + FIRST_PART_INDEX < argc) {
      token = strtok(argv[i + FIRST_PART_INDEX], ",");
      if (token == NULL)
        scream_and_quit("Invalid format: no START.");
      mbr.entries[i].lba_start = atoi(token);
      token = strtok(NULL, ",");
      if (token == NULL)
        scream_and_quit("Invalid format: no SIZE.");
      mbr.entries[i].sectors_count = atoi(token);
      token = strtok(NULL, ",");
      if (token == NULL)
        scream_and_quit("Invalid format: no TYPE.");
      mbr.entries[i].type = (unsigned char)(atoi(token));
      token = strtok(NULL, ",");
      if (token == NULL)
        scream_and_quit("Invalid format: no STATUS.");
      mbr.entries[i].status = atoi(token) ? PART_ACTIVE : PART_INACTIVE;
    }
  }

  mbr.signature = 0xaa55;

  f = fopen(DISK_IMG, "r+");
  if (f == NULL)
    scream_and_quit("Could not open disk image file.");
  if (fwrite(&mbr, 1, sizeof(struct mbr), f) != sizeof(struct mbr))
    scream_and_quit("Could not write mbr.");
  fclose(f);

  return 0;
}
