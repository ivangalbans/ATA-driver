#include <ata.h>
#include <typedef.h>
#include <fb.h>

ata_dev_t *devices = 0;
u8 lastid = 0;

void device_init()
{
	devices = (ata_dev_t*)malloc(64*sizeof(ata_dev_t));
	memset(devices, 0, 64*sizeof(ata_dev_t));
	lastid = 0;
	_kill();
}


void detail_dev(ata_dev_t* dev)
{
  fb_printf("present = %dd\n", dev->present);
  fb_printf("channel = %dd\n", dev->channel);
  fb_printf("drive = %dd\n", dev->drive);
  fb_printf("type = %dd\n", dev->type);
  fb_printf("signature = %dd\n", dev->signature);
  fb_printf("capabilities = %dd\n", dev->capabilities);
  fb_printf("commandsets = %dd\n", dev->commandsets);
  fb_printf("size = %dd\n", dev->size);
  fb_printf("model = %s\n", dev->model);
}

void device_print()
{
	int i;
	for(i = 0; i < lastid; ++i)
	{
		detail_dev(devices[i]);
	}
}