#include <ata.h>

/* Status */
#define ATA_SR_BSY                  0x80    /* Busy */
#define ATA_SR_DRDY                 0x40    /* Drive ready */
#define ATA_SR_DF                   0x20    /* Drive write fault */
#define ATA_SR_DSC                  0x10    /* Drive seek complete */
#define ATA_SR_DRQ                  0x08    /* Data request ready */
#define ATA_SR_CORR                 0x04    /* Corrected data */
#define ATA_SR_IDX                  0x02    /* Inlex */
#define ATA_SR_ERR                  0x01    /* Error */

/* Errors */
#define ATA_ER_BBK                  0x80    /* Bad sector */
#define ATA_ER_UNC                  0x40    /* Uncorrectable data */
#define ATA_ER_MC                   0x20    /* No media */
#define ATA_ER_IDNF                 0x10    /* ID mark not found */
#define ATA_ER_MCR                  0x08    /* No media */
#define ATA_ER_ABRT                 0x04    /* Command aborted */
#define ATA_ER_TK0NF                0x02    /* Track 0 not found */
#define ATA_ER_AMNF                 0x01    /* No address mark */

/* Commands */
#define ATA_CMD_READ_PIO            0x20    /* Read PIO mode, CHS/LBA28 */
#define ATA_CMD_READ_PIO_EXT        0x24    /* Read PIO mode, LBA48 */
#define ATA_CMD_READ_DMA            0xC8    /* Read DMA mode, LBA28 */
#define ATA_CMD_READ_DMA_EXT        0x25    /* Read DMA mode, LBA48 */
#define ATA_CMD_WRITE_PIO           0x30    /* Write PIO mode, CHS/LBA28 */
#define ATA_CMD_WRITE_PIO_EXT       0x34    /* Write PIO mode, LBA48 */
#define ATA_CMD_WRITE_DMA           0xCA    /* Write DMA mode, LBA28 */
#define ATA_CMD_WRITE_DMA_EXT       0x35    /* Write DMA mode, LBA48 */
#define ATA_CMD_CACHE_FLUSH         0xE7    /* Flush cache */
#define ATA_CMD_CACHE_FLUSH_EXT     0xEA    /* Flush cache and ? */
#define ATA_CMD_PACKET              0xA0    /* Packet (ATAPI perhaps?) */
#define ATA_CMD_IDENTIFY_PACKET     0xA1    /* Identify Packet (ATAPI?) */
#define ATA_CMD_IDENTIFY            0xEC    /* Identify */

/* ATAPI commands */
#define ATAPI_CMD_READ              0xA8    /* ATAPI read */
#define ATAPI_CMD_EJECT             0x1B    /* ATAPI eject 0==) */

/* Master/Slave selector */
#define ATA_DRIVE_SEL_MASTER        0x00    /* Used in ATA_REG_DEVSEL */
#define ATA_DRIVE_SEL_SLAVE         0x10

/* Select LBA mode */
#define ATA_USE_LBA                 0x40    /* Used in ATA_REG_DEVSEL */
#define ATA_USE_CHS                 0x00

/* Obsolete bits that must be set for backward compatibility */
#define ATA_OBSOLETE_1              0x80    /* Used in ATA_REG_DEVSEL */
#define ATA_OBSOLETE_2              0x20

/* ATA channels base addresses */
#define ATA_CH_PRI_BASE             0x01f0
#define ATA_CH_PRI_CONTROL_BASE     0x03f6
#define ATA_CH_SEC_BASE             0x0170
#define ATA_CH_SEC_CONTROL_BASE     0x0376

/* ATA standard ports.              Offset                  R/W     Width    */
/* ========================================================================= */
#define ATA_REG_DATA(base)          ((base) + 0x00)   /*    R/W      16      */
#define ATA_REG_ERROR(base)         ((base) + 0x01)   /*     R        8      */
#define ATA_REG_FEATURES(base)      ((base) + 0x01)   /*     W        8      */
#define ATA_REG_SECCOUNT0(base)     ((base) + 0x02)   /*    R/W       8      */
#define ATA_REG_LBA0(base)          ((base) + 0x03)   /*    R/W       8      */
#define ATA_REG_LBA1(base)          ((base) + 0x04)   /*    R/W       8      */
#define ATA_REG_LBA2(base)          ((base) + 0x05)   /*    R/W       8      */
#define ATA_REG_DEVSEL(base)        ((base) + 0x06)   /*    R/W       8      */
#define ATA_REG_COMMAND(base)       ((base) + 0x07)   /*     W        8      */
#define ATA_REG_STATUS(base)        ((base) + 0x07)   /*     R        8      */

/* ATA channel control ports.       Offset                   R/W    Width    */
/* ========================================================================= */
#define ATA_CH_REG_CONTROL(base)    ((base) + 0x00)   /*      W       8      */
#define ATA_CH_REG_ALTSTATUS(base)  ((base) + 0x00)   /*      R       8      */

/* Standard IRQs */
#define ATA_CH_PRI_IRQ              14
#define ATA_CH_SEC_IRQ              15


/* Initialize all ATA devices. */
int ata_init(ata_dev_t *devs[]) {
  /* TODO: Esta función debe devolver en el arreglo que se pasa como argumento
   *       la información de los cuatro posibles dispositivos ATA. En caso de
   *       que algún dispositivo no se encuentre, deberá rellenar los campos
   *       que lo ubican físicamente en al sistema y marcar el dispositivo
   *       como EMPTY.
   *       La información de cada dispositivo debe de obtenerse usando el
   *       comando IDENTIFY.
   *       En caso de utilizar interrupciones, este es el momento de establecer
   *       los manejadores de interrupciones.
   *       En caso de encontrarse algún error se deberá de retornar -1, en
   *       caso contrario 0. */
  return -1;
}

/* Read count sectors, starting at start, from dev into buf. */
int ata_read(ata_dev_t *dev, int start, int count, void *buf) {
  /* TODO: Esta función deberá leer count sectores, comenzando en el sector
   *       start, del dispositivo dev al buffer buf.
   *       Solo es necesario realizar la lectura en dispositivos ATA, no ATAPI.
   *       La función debe de garantizar que se ha leído todo lo que se
   *       solicitó, de lo contrario deberá reportar un error.
   *       0 como valor de retorno indica éxito, -1 indica fallo. */
  return -1;
}

int ata_write(ata_dev_t *dev, int start, int count, void *buf) {
  /* TODO: Esta función deberá escribir count sectores, comenzando en el sector
   *       start, hacia el dispositivo dev desde el buffer buf.
   *       Solo es necesario realizar la escritura en dispositivos ATA, no
   *       ATAPI. La función debe de garantizar que se ha escrito todo lo que
   *       se solicitó, de lo contrario deberá reportar un error.
   *       0 como valor de retorno indica éxito, -1 indica fallo. */
  return -1;
}
