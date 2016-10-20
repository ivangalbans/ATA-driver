/* Layout is as follows:

  Region                Size                  Description
  Boot code               1                   Left for boot code.
  Superblock              1                   Info about the filesystem.
  Inode map         sb.s_imap_blocks          Bitmap of inode usage.
  Zone map          sb.s_zmap_blocks          Bitmap of data zones usage.
  Inode table   inode_size*inodes/block_size  Inode table.
  Data zones          (remaining)             File and dir contents.
*/

/* This is just to avoid misunderstandings.
 *
 * THIS CODE IS PURPOSEDLY VERY, VERY UGLY!!! It's not supposed to
 * be read by anyone but an ashamed me.
 *
 * No refactoring, no layering, all you SHOULD NOT do when coding.
 * The reason is I don't to give a binary again to the students because
 * it's a pain to keep it in git and I really hate adding binaries to
 * a repository. So, I'll give this ugly code away. If they use it in
 * any form I think they'll get a terrible headache and their codes
 * will also become as ugly as this one. Though they can always learn
 * bad practices from it I won't apologize because, I repeat, STAY AWAY!
 *
 * I hope this file gets lost in buhos future history....
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "btool.h"

/* MINIX FS */

#define MINIX_BLOCK_SIZE      1024

#define MINIX_BLK2OFF(n)      (MINIX_BLOCK_SIZE * (n))

#define MINIX_SB              1
#define MINIX_INODE_MAP       2
#define MINIX_ZONE_MAP(sb)    (MINIX_INODE_MAP + (sb)->s_imap_blocks)
#define MINIX_INODE_TABLE(sb) (MINIX_ZONE_MAP((sb)) + (sb)->s_zmap_blocks)

#define MAP_GET_BIT(map, n)   (*((map) + (n) / 8) & ((char)(0x1) << ((n) % 8)))
#define MAP_SET_BIT(map, n)   (*((map) + (n) / 8) |= ((char)(0x1) << ((n) % 8)))
#define MAP_UNSET_BIT(map, n) do { \
                                /* printf("unsetting %hu\n", (n)); \
                                printf("%hhu => ", *((map) + (n) / 8)); */ \
                                (*((map) + (n) / 8) &= \
                                 ~(((char)(0x1) << ((n) % 8)))); \
                                /* printf(" %hhu\n", *((map) + (n) / 8));  */ \
                              } while (0);

#define MINIX_ROOT_INODE      1
#define MINIX_LINK_MAX        250
#define MINIX_MAX_NAME_LEN    30

#define MINIX_INODE_2_INODE_TABLE_BLOCK(i)  ((i) /  )

typedef struct _minix_sb_t_ {
  unsigned short s_inodes;
  unsigned short s_nzones;
  unsigned short s_imap_blocks;
  unsigned short s_zmap_blocks;
  unsigned short s_firstdatazone;
  unsigned short s_log_zone_size;
  unsigned int s_max_size;
  unsigned short s_magic;
  unsigned short s_state;
} __attribute__((packed)) minix_sb_t;

typedef struct _minix_inode_t_ {
  unsigned short i_mode;
  unsigned short i_uid;
  unsigned int i_size;
  unsigned int i_time;
  unsigned char i_gid;
  unsigned char i_nlinks;
  unsigned short i_zones[9];
} __attribute__((packed)) minix_inode_t;

#define MINIX_INODES_PER_BLOCK (MINIX_BLOCK_SIZE / sizeof(minix_inode_t))

typedef struct _minix_direntry_t_ {
  unsigned short inode;
  char name[MINIX_MAX_NAME_LEN];
} __attribute__((packed)) minix_direntry_t;

#define MINIX_DENTRIES_PER_BLOCK (MINIX_BLOCK_SIZE / sizeof(minix_direntry_t))

typedef struct _blk_ {
  unsigned short blk;
  off_t off;
  char data[MINIX_BLOCK_SIZE];
} block_t;

/* Globals */
int ps;           /* Partition start. */
minix_sb_t sb;    /* Superblock. */
FILE *img, *img2; /* img = disk image, img2 kernel image. */
char *inodes_map, *zones_map; /* inodes and zones bitmaps. */

void minix_load_block(block_t *blk) {
  blk->off = ps + MINIX_BLK2OFF(blk->blk);
  if (fseek(img, blk->off, SEEK_SET) == -1)
    scream_and_quit("Could not seek to block.");
  if (fread(blk->data, 1, MINIX_BLOCK_SIZE, img) != MINIX_BLOCK_SIZE)
    scream_and_quit("Could not read file block.");
}

void minix_store_block(block_t *blk) {
  blk->off = ps + MINIX_BLK2OFF(blk->blk);
  if (fseek(img, blk->off, SEEK_SET) == -1)
    scream_and_quit("Could not seek to flush block.");
  if (fwrite(blk->data, 1, MINIX_BLOCK_SIZE, img) != MINIX_BLOCK_SIZE)
    scream_and_quit("Could not flush block.");
}

unsigned short minix_file_block_2_block(minix_inode_t *inode, int b) {
  block_t blk;

  if (b * MINIX_BLOCK_SIZE > inode->i_size)
    scream_and_quit("Requested block is beyond EOF.");

  if (b < 7) {
    return inode->i_zones[b];
  }
  else if (b < 7 + 512) {
    blk.blk = inode->i_zones[7];
    minix_load_block(&blk);
    return ((unsigned short *)blk.data)[b - 7];
  }
  else {
    blk.blk = inode->i_zones[8];
    minix_load_block(&blk);
    blk.blk = ((unsigned short *)(blk.data))[(b - 519) / 512];
    minix_load_block(&blk);
    return ((unsigned short *)(blk.data))[(b - 519) % 512];
  }
}

unsigned short minix_add_file_block(minix_inode_t *inode) {
  int fb, k;
  block_t blk;

  fb = inode->i_size / MINIX_BLOCK_SIZE;
  if (inode->i_size % MINIX_BLOCK_SIZE)
    fb ++;

  if (fb < 7) {
    for (k = 0; k < sb.s_nzones && MAP_GET_BIT(zones_map, k); k ++);
    if (k == sb.s_nzones)
      scream_and_quit("No space left on device.");
    inode->i_zones[fb] = sb.s_firstdatazone + k;
    MAP_SET_BIT(zones_map, k);
    return sb.s_firstdatazone + k;
  }
  else if (fb < 7 + 512) {
    if (fb == 7) {
      for (k = 0; k < sb.s_nzones && MAP_GET_BIT(zones_map, k); k ++);
      if (k == sb.s_nzones)
        scream_and_quit("No space left on device.");
      inode->i_zones[7] = sb.s_firstdatazone + k;
      MAP_SET_BIT(zones_map, k);
    }
    blk.blk = inode->i_zones[7];
    minix_load_block(&blk);

    for (k = 0; k < sb.s_nzones && MAP_GET_BIT(zones_map, k); k ++);
    if (k == sb.s_nzones)
      scream_and_quit("No space left on device.");

    ((unsigned short *)blk.data)[fb - 7] = sb.s_firstdatazone + k;
    MAP_SET_BIT(zones_map, k);
    minix_store_block(&blk);

    return sb.s_firstdatazone + k;
  }
  else {
    if (fb == 7 + 512) {
      for (k = 0; k < sb.s_nzones && MAP_GET_BIT(zones_map, k); k ++);
      if (k == sb.s_nzones)
        scream_and_quit("No space left on device.");
      inode->i_zones[8] = sb.s_firstdatazone + k;
      MAP_SET_BIT(zones_map, k);
    }
    blk.blk = inode->i_zones[8];
    minix_load_block(&blk);

    if ((fb - 7 + 512) % 512 == 0) {
      for (k = 0; k < sb.s_nzones && MAP_GET_BIT(zones_map, k); k ++);
      if (k == sb.s_nzones)
        scream_and_quit("No space left on device.");
      ((unsigned short *)blk.data)[(fb - 7 - 512) / 512] = sb.s_firstdatazone + k;
      MAP_SET_BIT(zones_map, k);
      minix_store_block(&blk);
    }
    blk.blk = ((unsigned short *)blk.data)[(fb - 7 - 512) / 512];
    minix_load_block(&blk);

    for (k = 0; k < sb.s_nzones && MAP_GET_BIT(zones_map, k); k ++);
    if (k == sb.s_nzones)
      scream_and_quit("No space left on device.");
    ((unsigned short *)blk.data)[(fb - 7 - 512) % 512] = sb.s_firstdatazone + k;
    MAP_SET_BIT(zones_map, k);
    minix_store_block(&blk);

    return sb.s_firstdatazone + k;
  }
}

void minix_load_inode(int i, minix_inode_t *inode) {
  block_t blk;
  minix_inode_t *inodes;

  i = i - 1; /* MINIX inodes start counting in 1. */

  blk.blk = (unsigned short)(MINIX_INODE_TABLE(&sb) + i / MINIX_INODES_PER_BLOCK);
  minix_load_block(&blk);
  inodes = (minix_inode_t *)(blk.data);
  memcpy(inode, inodes + (i % MINIX_INODES_PER_BLOCK), sizeof(minix_inode_t));
}

void minix_save_inode(int i, minix_inode_t *inode) {
  block_t blk;
  minix_inode_t *inodes;

  i = i - 1; /* MINIX inodes start counting in 1. */

  blk.blk = (unsigned short)(MINIX_INODE_TABLE(&sb) + i / MINIX_INODES_PER_BLOCK);
  minix_load_block(&blk);
  inodes = (minix_inode_t *)(blk.data);
  memcpy(inodes + (i % MINIX_INODES_PER_BLOCK), inode, sizeof(minix_inode_t));
  minix_store_block(&blk);
}

void minix_trunc_inode(minix_inode_t *inode) {
  block_t blk, blk2;
  unsigned short *blks;
  int fb, i;

  for (i = 0; i < fb; i ++) {
    if (i < 7) {
      MAP_UNSET_BIT(zones_map, inode->i_zones[i] - sb.s_firstdatazone);
    }
    else if (i < 7 + 512) {
      if (i == 7) {
        blk.blk = inode->i_zones[7];
        minix_load_block(&blk);
        MAP_UNSET_BIT(zones_map, blk.blk - sb.s_firstdatazone);
      }
      MAP_UNSET_BIT(zones_map, ((unsigned short *)blk.data)[i - 7] - sb.s_firstdatazone);
    }
    else {
      if (i == 7 + 512) {
        blk.blk = inode->i_zones[8];
        minix_load_block(&blk);
        MAP_UNSET_BIT(zones_map, blk.blk - sb.s_firstdatazone);
      }
      if ((i - 7 - 512) % 512 == 0) {
        blk2.blk = ((unsigned short *)blk.data)[(i - 7 - 512) / 512];
        minix_load_block(&blk2);
        MAP_UNSET_BIT(zones_map, blk2.blk - sb.s_firstdatazone);
      }
      MAP_UNSET_BIT(zones_map, ((unsigned short *)blk2.data)[(i - 7 - 512) % 512] - sb.s_firstdatazone);
    }
  }
}

#define HELP_OPT      argv[2]

void minix_help(int argc, char *argv[]) {
  printf(
"usage: %s %s DISK_IMAGE KERNEL [ PARTNO ]\n"
"       DISK_IMAGE : path to the full disk image file.\n"
"       KERNEL     : path to the kernel image to install at PARTNO.\n"
"       PARTNO     : The partition to install the kernel at. If not\n"
"                    present, the active partition is selected.\n"
  , CMD, HELP_OPT);
}

int minix_write(int argc, char *argv[]) {
  struct mbr mbr;
  minix_inode_t inode, root;
  minix_direntry_t *dentries, dentry;
  minix_inode_t *inodes;
  unsigned short *zones;
  unsigned short b;
  block_t bmain, bsec, bthird;

  #define imgf      argv[2]
  #define kerf      argv[3]
  #define _partno   argv[4]

  int partno, br, done;
  size_t r;
  struct stat kstat;
  int kerb, i, k, count, slb;

  /* Load MBR */
  img = fopen(imgf, "r+");
  if (img == NULL)
    scream_and_quit("Could not open image file.");
  if (fread(&mbr, 1, sizeof(struct mbr), img) != sizeof(struct mbr))
    scream_and_quit("Could not read mbr.");

  if (argc > 4) {
    partno = atoi(_partno) - 1;
  }
  else {
    for (i = 0; i < 4; i ++) {
      if (mbr.entries[i].status & PART_ACTIVE) {
        partno = i;
        break;
      }
    }
  }
  if (partno < 0 || partno > 3) {
    scream_and_quit("Partition number must be in range 1 - 4.");
  }

  /* Let's check the partition is marked as MINIX FS */
  if (mbr.entries[partno].type != 0x81) {
    debug_uc(mbr.entries[partno].type);
    scream_and_quit("Selected partition is not a valid MINIX FS partition.");
  }

  /* Compute partition start address. */
  ps = LBA2OFF(mbr.entries[partno].lba_start);

  /* Get kernel stats. */
  if (stat(kerf, &kstat) == -1)
    scream_and_quit("Could not get kernel stats.");

  debug_sl(kstat.st_size);
  debug_ui(sb.s_max_size);
  debug_us(sb.s_nzones);
  debug_us(sb.s_inodes);


  /*** MINIX ***/

  /* Load superblock. */
  bmain.blk = MINIX_SB;
  minix_load_block(&bmain);
  memcpy(&sb, bmain.data, sizeof(minix_sb_t));

  /* Load inodes and zones maps. */
  inodes_map = (char *)malloc(sb.s_imap_blocks * MINIX_BLOCK_SIZE);
  if (inodes_map == NULL)
    scream_and_quit("Could not allocate memory for inodes map.");
  if (fseek(img, ps + MINIX_BLK2OFF(MINIX_INODE_MAP), SEEK_SET) == -1)
    scream_and_quit("Could not seek to inodes map.");
  if (fread(inodes_map, 1, sb.s_imap_blocks * MINIX_BLOCK_SIZE, img) !=
      sb.s_imap_blocks * MINIX_BLOCK_SIZE)
    scream_and_quit("Could not read inodes map.");

  zones_map = (char *)malloc(sb.s_zmap_blocks * MINIX_BLOCK_SIZE);
  if (zones_map == NULL)
    scream_and_quit("Could not allocate memory for zones map.");
  if (fseek(img, ps + MINIX_BLK2OFF(MINIX_ZONE_MAP(&sb)), SEEK_SET) == -1)
    scream_and_quit("Could not seek to zones map.");
  if (fread(zones_map, 1, sb.s_zmap_blocks * MINIX_BLOCK_SIZE, img) !=
      sb.s_zmap_blocks * MINIX_BLOCK_SIZE)
    scream_and_quit("Could not read zones map.");

  /* Let's try to find the file /kernel */
  /* First, let's locate the root inode, which is the first inode in MINIX. */
  minix_load_inode(MINIX_ROOT_INODE, &root);

  /* Scan root inode looking for "kernel" and delete it if found. */
  for (i = 0; i < (root.i_size - 1) / MINIX_BLOCK_SIZE + 1; i++) {
    bmain.blk = minix_file_block_2_block(&root, i);
    minix_load_block(&bmain);

    /* Check the existence of /kernel. */
    for (k = 0, dentries = (minix_direntry_t *)bmain.data;
         k < MINIX_DENTRIES_PER_BLOCK &&
         i * MINIX_BLOCK_SIZE + k * sizeof(minix_direntry_t) < root.i_size;
         k ++) {
      if (dentries[k].inode != 0 &&
          memcmp("kernel", dentries[k].name, strlen("kernel") + 1) == 0) {
        /* Load inode */
        minix_load_inode(dentries[k].inode, &inode);
        /* Truncate it */
        minix_trunc_inode(&inode);
        /* Release it. */
        memset(&inode, 0, sizeof(minix_inode_t));
        minix_save_inode(dentries[k].inode, &inode);
        MAP_UNSET_BIT(inodes_map, dentries[k].inode);
        /* Clear the entry. */
        dentries[k].inode = 0; /* Mark it as a hole. */
        memset(dentries[k].name, 0, strlen("kernel"));
        /* And flush it. Now we can reuse bmain. */
        minix_store_block(&bmain);

        /* Cause the outer loop execution to stop. */
        i = (root.i_size - 1) / MINIX_BLOCK_SIZE + 1;
        break;
      }
    }
  }

  /* Check if we can store the kernel. */
  kerb = ( kstat.st_size - 1 ) / MINIX_BLOCK_SIZE + 1;
  if (kerb >= 7 && kerb < 7 + 512)
    kerb ++;
  else if (kerb >= 512 + 7)
    kerb += 2 + ( ( ( kerb - 7 - 512 ) - 1 ) / 512 + 1 );

  for (count = 0, i = 0; i < sb.s_nzones; i ++) {
    count += MAP_GET_BIT(zones_map, i) == 0 ? 1 : 0;
    if (count == kerb)
      break;
  }
  if (count < kerb)
    scream_and_quit("There's not free space for the kernel");

  /* Check if there's a free inode to hold the kernel. */
  for (i = 2; i < sb.s_inodes && MAP_GET_BIT(inodes_map, i); i ++);
  if (i == sb.s_inodes)
    scream_and_quit("Could not find a free spot in the inode table.");
  MAP_SET_BIT(inodes_map, i);

  /* Prepare the new inode for kernel. */
  memset(&inode, 0, sizeof(minix_inode_t));
  inode.i_mode = S_IFREG | S_IRUSR | S_IWUSR;
  inode.i_uid = 0;
  inode.i_gid = 0;
  inode.i_nlinks = 1;
  inode.i_size = 0;
  inode.i_time = time(NULL);
  memset(&(inode.i_zones), 0, sizeof(unsigned short) * 9);
  minix_save_inode(i, &inode);

  /* Open kernel file. */
  img2 = fopen(kerf, "r");
  if (img2 == NULL)
    scream_and_quit("Could not open kernel image.");

  /* Copy every kernel's block to the partition. */
  for (k = 0; k < ( kstat.st_size - 1 ) / MINIX_BLOCK_SIZE + 1; k ++) {
    memset(bmain.data, 0, MINIX_BLOCK_SIZE);
    br = fread(bmain.data, 1, MINIX_BLOCK_SIZE, img2);
    if (br != MINIX_BLOCK_SIZE && ferror(img2))
      scream_and_quit("Could not read data from kernel image.");
    bmain.blk = minix_add_file_block(&inode);
    minix_store_block(&bmain);
    inode.i_size += br;
    minix_save_inode(i, &inode);
  }

  fclose(img2);

  /* Prepare dentry for "kernel". */
  memset(&dentry, 0, sizeof(minix_direntry_t));
  dentry.inode = i;
  strcpy(dentry.name, "kernel");

  /* Load / */
  minix_load_inode(MINIX_ROOT_INODE, &root);
  /* Scan / looking for a free slot. */
  for (i = 0, done = 0;
       !done && i < (root.i_size - 1) / MINIX_BLOCK_SIZE + 1;
       i++) {
    bmain.blk = minix_file_block_2_block(&root, i);
    minix_load_block(&bmain);
    dentries = (minix_direntry_t *)bmain.data;
    for (k = 0; k < MINIX_DENTRIES_PER_BLOCK; k ++) {
      if (dentries[k].inode == 0) {
        dentries[k] = dentry;
        minix_store_block(&bmain);
        if (i * MINIX_BLOCK_SIZE + k * sizeof(minix_direntry_t) >= root.i_size) {
          root.i_size += sizeof(minix_direntry_t);
          minix_save_inode(MINIX_ROOT_INODE, &root);
        }
        done = 1;
        break;
      }
    }
  }
  if (!done) {
    bmain.blk = minix_add_file_block(&root);
    memcpy(bmain.data, &dentry, sizeof(minix_direntry_t));
    minix_store_block(&bmain);
    root.i_size += sizeof(minix_direntry_t);
    minix_save_inode(MINIX_ROOT_INODE, &root);
  }

  /* Save inodes and zones maps. */
  if (fseek(img, ps + MINIX_BLK2OFF(MINIX_INODE_MAP), SEEK_SET) == -1)
    scream_and_quit("Could not seek to inodes map.");
  if (fwrite(inodes_map, 1, sb.s_imap_blocks * MINIX_BLOCK_SIZE, img) !=
      sb.s_imap_blocks * MINIX_BLOCK_SIZE)
    scream_and_quit("Could not write inodes map.");

  if (fseek(img, ps + MINIX_BLK2OFF(MINIX_ZONE_MAP(&sb)), SEEK_SET) == -1)
    scream_and_quit("Could not seek to zones map.");
  if (fwrite(zones_map, 1, sb.s_zmap_blocks * MINIX_BLOCK_SIZE, img) !=
      sb.s_zmap_blocks * MINIX_BLOCK_SIZE)
    scream_and_quit("Could not write zones map.");

  fclose(img);

  return 0;
}
