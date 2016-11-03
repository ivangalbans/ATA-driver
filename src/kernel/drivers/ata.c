#include <ata.h>
#include <fb.h>
#include <io.h>
#include <device.h>
#include <string.h>
#include <mem.h>

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

/* Words in the Identification Space*/
#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

/* ATAPI commands */
#define ATA_IDENTIFY_CMD_MASTER     0xA0
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
#define ATA_CH_THIRD_BASE           0x01E8
#define ATA_CH_THIRD_CONTROL_BASE   0x03E6
#define ATA_CH_FOURTH_BASE          0x0168
#define ATA_CH_FOURTH_CONTROL_BASE  0x0366

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


u16 ata_bus_port[] = {ATA_CH_PRI_BASE, ATA_CH_SEC_BASE, 
                      ATA_CH_THIRD_BASE, ATA_CH_FOURTH_BASE};

u16 ata_bus_control[] = {ATA_CH_PRI_CONTROL_BASE, ATA_CH_SEC_CONTROL_BASE, 
                          ATA_CH_THIRD_CONTROL_BASE, ATA_CH_FOURTH_CONTROL_BASE};

void detail_dev(ata_dev_t* dev)
{
  /*fb_printf("present = %dd\n", dev->present);
  fb_printf("channel = %dd\n", dev->channel);
  fb_printf("drive = %dd\n", dev->drive);
  fb_printf("type = %dd\n", dev->type);
  fb_printf("signature = %dd\n", dev->signature);
  fb_printf("capabilities = %dd\n", dev->capabilities);
  fb_printf("commandsets = %dd\n", dev->commandsets);
  fb_printf("size = %dd\n", dev->size);*/
  fb_write(dev->model, strlen(dev->model));
  fb_printf("\n");
  
}

void delay(u16 dev, int ms)
{
  ms = ms/100;
  while(ms--)
    inb(ATA_REG_STATUS(dev));
}

//Detect ATA-ATAPI Devices:
u8 identify_command(ata_dev_t * dev, u8 idx, char * buffer)
{
  u8 lba1, lba2;
  u8 status = 0, error;
  u8 _channel = idx / 2;          /* ATA_CHANNEL_* */
  u8 _drive = idx % 2;
  u16 i;

  // (I) Select Drive:
  outb(ATA_REG_DEVSEL(ata_bus_port[_channel]), ATA_IDENTIFY_CMD_MASTER | ((idx % 2) << 4) );

  /* ATA specs say these values must be zero before sending IDENTIFY */
  outb(ATA_REG_SECCOUNT0(ata_bus_port[_channel]), 0);
  outb(ATA_REG_LBA0(ata_bus_port[_channel]), 0);
  outb(ATA_REG_LBA1(ata_bus_port[_channel]), 0);
  outb(ATA_REG_LBA2(ata_bus_port[_channel]), 0);

  // (II) Send ATA Identify Command:
  outb(ATA_REG_COMMAND(ata_bus_port[_channel]), ATA_CMD_IDENTIFY);
  delay(ata_bus_control[_channel], 400);

  /*read status port */
  status = inb(ATA_REG_STATUS(ata_bus_port[_channel]));

  if(!status)
  {
    dev->present = ATA_DEVICE_EMPTY;
    return 0;
  }

  dev->present = ATA_DEVICE_PRESENT;
  // (III) Polling:
  while(TRUE)
  {
      status = inb(ATA_REG_STATUS(ata_bus_port[_channel]));
      if((status & ATA_SR_ERR) || (status & ATA_SR_DF))// If Err, Device is not ATA.
      {
        error = 1;
        break;
      }

      if(!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) // Everything is right.
        break;
  }

  dev->type = ATA_TYPE_ATA;
  if(error)
  {
    lba1 = inb(ATA_REG_LBA1(ata_bus_port[_channel]));
    lba2 = inb(ATA_REG_LBA2(ata_bus_port[_channel]));
    
    if( (lba1 == 0x14 && lba2 == 0xeb) || (lba1 == 0x69 && lba2 == 0x96))
      dev->type = ATA_TYPE_ATAPI;
    else 
      return -1;

    outb(ATA_REG_COMMAND(ata_bus_port[_channel]),ATA_CMD_IDENTIFY_PACKET);
    delay(ata_bus_control[_channel], 400);
  }


  for(i = 0; i < 256; ++i)
    *(u16*)(buffer + i*2) = inw(ATA_REG_DATA(ata_bus_port[_channel]));

  dev->channel = _channel;
  dev->drive = _drive;
  dev->signature    = *((u16*) (buffer + ATA_IDENT_DEVICETYPE));
  dev->capabilities = *((u16*) (buffer + ATA_IDENT_CAPABILITIES));
  dev->commandsets  = *((u32*) (buffer + ATA_IDENT_COMMANDSETS));

  // (VII) Get Size:
  if (dev->commandsets & (1 << 26))
      // Device uses 48-Bit Addressing:
      dev->size   = *((u32*) (buffer + ATA_IDENT_MAX_LBA_EXT));
  else
     // Device uses CHS or 28-bit Addressing:
     dev->size = *((u32*) (buffer + ATA_IDENT_MAX_LBA));

  /* Model goes from W#27 to W#46 */
  for(i = 0; i < 40; i += 2)
  {

    dev->model[i] = buffer[ATA_IDENT_MODEL + i + 1];
    dev->model[i+1] = buffer[ATA_IDENT_MODEL + i];
  }
  dev->model[40] = '\0';

  return 0;
}



/* Initialize all ATA devices. */
int ata_init(ata_dev_t* devs[])
{
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

   char buffer[512];
   u8 i, error = 0;
   for(i = 0; i < 4; ++i)
   {
      fb_printf("Dev: %dd \n", i);   
      if(identify_command(devs[i], i, buffer)){
        fb_printf("ERROR\n");
        error = -1;
      }
      else if(devs[i]->present)
        detail_dev(devs[i]);
      else
        fb_printf("DEVICE EMPTY\n");
   }

  return error;
}

int poll(int channel)
{
  u8 status;
  delay(channel, 400);

  status = inb(ATA_REG_STATUS(channel));

  while(status & ATA_SR_BSY)
    status = inb(ATA_REG_STATUS(channel));

  while(TRUE)
  {
    if((status & ATA_SR_ERR) || (status & ATA_SR_DF)){
      fb_printf("Operation Errors");
      return -1;
  }
    if((status & ATA_SR_DRQ))
      return 0;
    status = inb(ATA_REG_STATUS(channel));
  }
  return 0;
}

/* Read count sectors, starting at start, from dev into buf. */
int ata_read(ata_dev_t *dev, int start, int count, void *buf) {
  /* TODO: Esta función deberá leer count sectores, comenzando en el sector
   *       start, del dispositivo dev al buffer buf.
   *       Solo es necesario realizar la lectura en dispositivos ATA, no ATAPI.
   *       La función debe de garantizar que se ha leído todo lo que se
   *       solicitó, de lo contrario deberá reportar un error.
   *       0 como valor de retorno indica éxito, -1 indica fallo. */
  int i, j;
  u16 ch = ata_bus_port[dev->channel];
  u16 *ptr = (u16*)buf;

  while(inb(ATA_REG_STATUS(ch)) & ATA_SR_BSY);

  outb(ATA_REG_DEVSEL(ch), 0xE0 | ((dev->drive) << 4) );
  //outb(ATA_REG_ERROR(ch), 0);
  outb(ATA_REG_SECCOUNT0(ch), count);
  outb(ATA_REG_LBA0(ch),(start & 0x000000FF));
  outb(ATA_REG_LBA1(ch),((start & 0x0000FF00)>>8));
  outb(ATA_REG_LBA2(ch),((start & 0x00FF0000)>>16));
  outb(ATA_REG_COMMAND(ch),ATA_CMD_READ_PIO);

  for(i = 0; i < count; ++i)
  {
    if(poll(ch))
      return -1;
    for(j = 0; j < 256; ++j)
      ptr[i*256 + j] = inw(ATA_REG_DATA(ch));
  }

  return 0;
}

int ata_write(ata_dev_t *dev, int start, int count, void *buf) {
  /* TODO: Esta función deberá escribir count sectores, comenzando en el sector
   *       start, hacia el dispositivo dev desde el buffer buf.
   *       Solo es necesario realizar la escritura en dispositivos ATA, no
   *       ATAPI. La función debe de garantizar que se ha escrito todo lo que
   *       se solicitó, de lo contrario deberá reportar un error.
   *       0 como valor de retorno indica éxito, -1 indica fallo. */
  
  int i, j;
  u16 ch = ata_bus_port[dev->channel];
  u16 *ptr = (u16 *)buf;

  while(inb(ATA_REG_STATUS(ch)) & ATA_SR_BSY);

  outb(ATA_REG_DEVSEL(ch), 0xE0 | ((dev->drive) << 4) );
  outb(ATA_REG_ERROR(ch), 0);
  outb(ATA_REG_SECCOUNT0(ch), (unsigned char)count);
  outb(ATA_REG_LBA0(ch),(unsigned char)(start & 0x000000FF));
  outb(ATA_REG_LBA1(ch),(unsigned char)((start & 0x0000FF00)>>8));
  outb(ATA_REG_LBA2(ch),(unsigned char)((start & 0x00FF0000)>>16));
  outb(ATA_REG_COMMAND(ch),ATA_CMD_WRITE_PIO);

  for(i = 0;i < count; ++i)
  {
    if(poll(ch))
      return -1;
    for(j = 0;j < 256 ; ++j)
      outw(ATA_REG_DATA(ch),ptr[i*256 + j]);
  }
  
  return 0;

}
