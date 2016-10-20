/* Debugging things */

#ifdef __DEBUG__

#define debug(s)      printf(s)
#define debug_s(s)    printf(#s ": %s\n", s)
#define debug_us(n)   printf(#n ": %hu\n", n)
#define debug_ul(n)   printf(#n ": %lu\n", n)
#define debug_sl(n)   printf(#n ": %ld\n", n)
#define debug_uc(n)   printf(#n ": %hhu\n", n)
#define debug_uos(n)  printf(#n ": %ho\n", n)
#define debug_si(n)   printf(#n ": %d\n", n)
#define debug_ui(n)   printf(#n ": %u\n", n)

#else

#define debug(s)      do {} while (0)
#define debug_s(s)    do {} while (0)
#define debug_us(n)   do {} while (0)
#define debug_ul(n)   do {} while (0)
#define debug_sl(n)   do {} while (0)
#define debug_uc(n)   do {} while (0)
#define debug_uos(n)  do {} while (0)
#define debug_si(n)   do {} while (0)
#define debug_ui(n)   do {} while (0)

#endif

/* Common disk stuff */

#define SECTOR_SIZE         512
#define LBA2OFF(n)          (SECTOR_SIZE * (n))
#define PART_ACTIVE         0x80
#define PART_INACTIVE       0x00

struct mbr_entry {
  unsigned char status;
  unsigned char chs_start[3];
  unsigned char type;
  unsigned char chs_end[3];
  unsigned int lba_start;
  unsigned int sectors_count;
}__attribute__((__packed__));

struct mbr {
  unsigned char _pad[440];
  unsigned int disk_id;
  unsigned short _pad2;       /* TODO: Check what's this. */
  struct mbr_entry entries[4];
  unsigned short signature;
}__attribute__((__packed__));


/* Common command stuff. */

#define PROG                argv[0]
#define CMD                 argv[1]

/* btool.c */
void scream_and_quit(char *);

/* mbr.c */
int mbr_write(int, char *[]);

/* bootloader.c */
int bootloader_write(int, char *[]);

/* minix.c */
int minix_write(int, char *[]);
