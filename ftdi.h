#ifndef FTDI_MODULE_H
#define FTDI_MODULE_H

#include <linux/usb.h>
#include <linux/gpio/driver.h>

enum ftdi_driver_mode_types {
	FTDI_MODE_I2C = 0,
	FTDI_MODE_GPIO,
	FTDI_MODE_SERIAL,
	FTDI_MODE_SPI,
	FTDI_MODE_ENUM_END,
};

struct ftdi_priv {
	struct usb_device *usb_dev;
	enum ftdi_driver_mode_types mode;
#ifdef CONFIG_GPIOLIB
    struct gpio_chip gc;
#endif
};

static int ftdi_gpio_register(struct usb_interface *interface);

static int ftdi_gpio_deregister(struct usb_interface *interface);

#endif // !FTDI_MODULE_H
