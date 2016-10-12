#ifndef __SERIAL_H_
#define __SERIAL_H_

#include <typedef.h>

typedef u16                           serial_device_t;

#define SERIAL_COM1                   0x03f8
#define SERIAL_COM2                   0x02f8
#define SERIAL_COM3                   0x03e8
#define SERIAL_COM4                   0x02e8

/* Interrupt related values. */
#define SERIAL_INT_NONE               0x00
#define SERIAL_INT_DATA_AVAILABLE     0x01
#define SERIAL_INT_TRANSMITER_EMPTY   0x02
#define SERIAL_INT_BREAK_OR_ERROR     0x04
#define SERIAL_INT_STATUS_CHANGE      0x08

/* Line control related values. */
#define SERIAL_CHARACTER_LENGTH_5     0x00
#define SERIAL_CHARACTER_LENGTH_6     0x01
#define SERIAL_CHARACTER_LENGTH_7     0x02
#define SERIAL_CHARACTER_LENGTH_8     0x03

#define SERIAL_SINGLE_STOP_BIT        0x00
#define SERIAL_DOUBLE_STOP_BITS       0x04  /* It's 1.5 if char len is 5 */

#define SERIAL_PARITY_NONE            0x00
#define SERIAL_PARITY_ODD             0x04
#define SERIAL_PARITY_EVEN            0x14
#define SERIAL_PARITY_MARK            0x24
#define SERIAL_PARITY_SPACE           0x34

typedef struct serial_port_config {
  u16 divisor;
  u8  available_interrupts;
  u8  line_config;    /* This comprehends char len, stop bit and parity. */
} serial_port_config_t;

/* Initialize the serial port */
int serial_init(serial_device_t dev, serial_port_config_t *config); // u16 divisor, u8 int_flags, u8 Fifo, u16 divisor, u8 lenPrtyStop, u8 IRQ);

/* Writes len bytes from buf into the serial port. */
void serial_write(serial_device_t dev, void *buf, u32 len);

u32 serial_read(serial_device_t dev, void *buf, u32 len);

#endif
