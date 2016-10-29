#ifndef __ATA_H__
#define __ATA_H__

#include <typedef.h>

#define ATA_DEVICE_EMPTY          0x00
#define ATA_DEVICE_PRESENT        0x01
#define ATA_CHANNEL_PRIMARY       0x00
#define ATA_CHANNEL_SECONDARY     0x01
#define ATA_DRIVE_MASTER          0x00
#define ATA_DRIVE_SLAVE           0x01
#define ATA_TYPE_ATA              0x00
#define ATA_TYPE_ATAPI            0x01
#define ATA_SIZE

/* Public ATA device structure. */
typedef struct ata_dev {
  u8 present;         /* ATA_DEVICE_* */
  u8 channel;         /* ATA_CHANNEL_* */
  u8 drive;           /* ATA_DRIVE_* */
  u16 type;           /* ATA_TYPE_* */
  u16 signature;      /* Drive Signature */
  u16 capabilities;   /* Features */
  u32 commandsets;    /* Supported Command Sets */
  u32 size;           /* Size in sectors. */
  char model[41];     /* Model in string. */
} ata_dev_t;

void detail_dev(ata_dev_t*);
void delay(u16, u8);
u8 identify_command(ata_dev_t *, u8, u8*);
int ata_init();
int ata_read(ata_dev_t *, int, int, void *);
int ata_write(ata_dev_t *, int, int, void *);

#endif
