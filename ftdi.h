#ifndef FTDI_MODULE_H
#define FTDI_MODULE_H

#include <linux/usb.h>
#include <linux/gpio/driver.h>

/* Commands */
#define FTDI_SIO_SET_BITMODE		0x0b /* Set bitbang mode */
#define FTDI_SIO_READ_PINS		0x0c /* Read immediate value of pins */
#define FTDI_SIO_READ_EEPROM		0x90 /* Read EEPROM */

/* Possible bitmodes for FTDI_SIO_SET_BITMODE_REQUEST */
#define FTDI_SIO_BITMODE_RESET		0x00
#define FTDI_SIO_BITMODE_CBUS		0x20

/* FTDI_SIO_SET_BITMODE */
#define FTDI_SIO_SET_BITMODE_REQUEST_TYPE 0x40
#define FTDI_SIO_SET_BITMODE_REQUEST FTDI_SIO_SET_BITMODE


/* Possible bitmodes for FTDI_SIO_SET_BITMODE_REQUEST */
#define FTDI_SIO_BITMODE_RESET		0x00
#define FTDI_SIO_BITMODE_CBUS		0x20


/* FTDI_SIO_READ_PINS */
#define FTDI_SIO_READ_PINS_REQUEST_TYPE 0xc0
#define FTDI_SIO_READ_PINS_REQUEST FTDI_SIO_READ_PINS



#define FTDI_SIO_READ_EEPROM_REQUEST_TYPE 0xc0
#define FTDI_SIO_READ_EEPROM_REQUEST FTDI_SIO_READ_EEPROM

#define FTDI_FTX_CBUS_MUX_GPIO		0x8
#define FTDI_FT232R_CBUS_MUX_GPIO	0xa

#define WDR_TIMEOUT 5000 /* default urb timeout */


enum ftdi_driver_mode_types {
	FTDI_MODE_I2C = 0,
	FTDI_MODE_GPIO,
	FTDI_MODE_SERIAL,
	FTDI_MODE_SPI,
	FTDI_MODE_ENUM_END,
};

struct ftdi_priv {
	struct usb_device *usb_dev;         /* usb device pointer*/
	enum ftdi_driver_mode_types mode;   /* mode of the device*/
    u16 channel;		/* channel index, or 0 for legacy types */

#ifdef CONFIG_GPIOLIB
    struct gpio_chip gc;
    struct mutex gpio_lock;	/* protects GPIO state */
    bool gpio_registered;	/* is the gpiochip in kernel registered */
    bool gpio_used;		/* true if the user requested a gpio */
    u8 gpio_altfunc;	/* which pins are in gpio mode */
    u8 gpio_output;		/* pin directions cache */
    u8 gpio_value;		/* pin value for outputs */

#endif
};

int ftdi_gpio_register(struct usb_interface *interface);

void ftdi_gpio_deregister(struct usb_interface *interface);

int ftdi_read_eeprom(struct usb_interface *interface, void *dst, u16 addr, u16 nbytes);

#endif // !FTDI_MODULE_H
