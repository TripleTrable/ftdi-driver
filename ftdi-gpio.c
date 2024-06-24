#include "linux/kern_levels.h"
#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/usb.h>
#include "ftdi.h"

/* ----------------------------------------------------------------------------
 *                             FTDI gpio driver
 * ----------------------------------------------------------------------------
 */
static int ftdi_set_bitmode(struct usb_interface *interface, u8 mode)
{
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	struct usb_device *dev = priv->usb_dev;
	int result;
	u16 val;

	result = usb_autopm_get_interface(interface);
	if (result)
		return result;

	val = (mode << 8) | (priv->gpio_output << 4) | priv->gpio_value;
	result = usb_control_msg(dev,
				 usb_sndctrlpipe(dev, 0),
				 FTDI_SIO_SET_BITMODE_REQUEST,
				 FTDI_SIO_SET_BITMODE_REQUEST_TYPE, val,
				 priv->channel, NULL, 0, WDR_TIMEOUT);
	if (result < 0) {
		dev_err(&interface->dev,
			"bitmode request failed for value 0x%04x: %d\n",
			val, result);
	}

	usb_autopm_put_interface(interface);

	return result;
}


static int ftdi_set_cbus_pins(struct usb_interface *interface)
{
	return ftdi_set_bitmode(interface, FTDI_SIO_BITMODE_CBUS);
}

static int ftdi_read_cbus_pins(struct usb_interface *interface)
{
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	struct usb_device *dev = priv->usb_dev;
	u8 buf;
	int result;

	result = usb_autopm_get_interface(interface);
	if (result)
		return result;

	result = usb_control_msg_recv(dev, 0,
				      FTDI_SIO_READ_PINS_REQUEST,
				      FTDI_SIO_READ_PINS_REQUEST_TYPE, 0,
				      priv->channel, &buf, 1, WDR_TIMEOUT,
				      GFP_KERNEL);
	if (result == 0)
		result = buf;

	usb_autopm_put_interface(interface);

	return result;
}

static int ftdi_exit_cbus_mode(struct usb_interface *interface)
{
	struct ftdi_priv *priv = usb_get_intfdata(interface);

	priv->gpio_output = 0;
	priv->gpio_value = 0;
	return ftdi_set_bitmode(interface, FTDI_SIO_BITMODE_RESET);
}


static int ftdi_gpio_request(struct gpio_chip *gc, unsigned int offset)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	int result;

	mutex_lock(&priv->gpio_lock);
	if (!priv->gpio_used) {
		/* Set default pin states, as we cannot get them from device */
		priv->gpio_output = 0x00;
		priv->gpio_value = 0x00;
		result = ftdi_set_cbus_pins(interface);
		if (result) {
			mutex_unlock(&priv->gpio_lock);
			return result;
		}

		priv->gpio_used = true;
	}
	mutex_unlock(&priv->gpio_lock);

	return 0;
}

static int ftdi_gpio_direction_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);

	return !(priv->gpio_output & BIT(gpio));
}

static int ftdi_gpio_direction_input(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	int result;

	mutex_lock(&priv->gpio_lock);

	priv->gpio_output &= ~BIT(gpio);
	result = ftdi_set_cbus_pins(interface);

	mutex_unlock(&priv->gpio_lock);

	return result;
}

static int ftdi_gpio_direction_output(struct gpio_chip *gc, unsigned int gpio,
					int value)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	int result;

	mutex_lock(&priv->gpio_lock);

	priv->gpio_output |= BIT(gpio);
	if (value)
		priv->gpio_value |= BIT(gpio);
	else
		priv->gpio_value &= ~BIT(gpio);

	result = ftdi_set_cbus_pins(interface);

	mutex_unlock(&priv->gpio_lock);

	return result;
}

static int ftdi_gpio_init_valid_mask(struct gpio_chip *gc,
				     unsigned long *valid_mask,
				     unsigned int ngpios)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	unsigned long map = priv->gpio_altfunc;

	bitmap_complement(valid_mask, &map, ngpios);

	if (bitmap_empty(valid_mask, ngpios))
		dev_dbg(&interface->dev, "no CBUS pin configured for GPIO\n");
	else
		dev_dbg(&interface->dev, "CBUS%*pbl configured for GPIO\n", ngpios,
			valid_mask);

	return 0;
}


static int ftdi_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	int result;

	result = ftdi_read_cbus_pins(interface);
	if (result < 0)
		return result;

	return !!(result & BIT(gpio));
}

static void ftdi_gpio_set(struct gpio_chip *gc, unsigned int gpio, int value)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);

	mutex_lock(&priv->gpio_lock);

	if (value)
		priv->gpio_value |= BIT(gpio);
	else
		priv->gpio_value &= ~BIT(gpio);

	ftdi_set_cbus_pins(interface);

	mutex_unlock(&priv->gpio_lock);
}


static int ftdi_gpio_get_multiple(struct gpio_chip *gc, unsigned long *mask,
					unsigned long *bits)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	int result;

	result = ftdi_read_cbus_pins(interface);
	if (result < 0)
		return result;

	*bits = result & *mask;

	return 0;
}

static void ftdi_gpio_set_multiple(struct gpio_chip *gc, unsigned long *mask,
					unsigned long *bits)
{
	struct usb_interface *interface = gpiochip_get_data(gc);
	struct ftdi_priv *priv = usb_get_intfdata(interface);

	mutex_lock(&priv->gpio_lock);

	priv->gpio_value &= ~(*mask);
	priv->gpio_value |= *bits & *mask;
	ftdi_set_cbus_pins(interface);

	mutex_unlock(&priv->gpio_lock);
}

/* ----------------------------------------------------------------------------
 *                             Init stuff for gpio
 * ----------------------------------------------------------------------------
 */

static int ftdi_gpio_init_ft232h(struct usb_interface *interface)
{
	struct ftdi_priv *priv = usb_get_intfdata(interface);
	u16 cbus_config;
	u8 *buf;
	int ret;
	int i;

	buf = kmalloc(4, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = ftdi_read_eeprom(interface, buf, 0x1a, 4);
	if (ret < 0)
		goto out_free;

	/*
	 * FT232H CBUS Memory Map
	 *
	 * 0x1a: X- (upper nibble -> AC5)
	 * 0x1b: -X (lower nibble -> AC6)
	 * 0x1c: XX (upper nibble -> AC9 | lower nibble -> AC8)
	 */
	cbus_config = buf[2] << 8 | (buf[1] & 0xf) << 4 | (buf[0] & 0xf0) >> 4;

    printk(KERN_WARNING "cbus config is %04x",cbus_config);
	priv->gc.ngpio = 4;
	priv->gpio_altfunc = 0xff;

	for (i = 0; i < priv->gc.ngpio; ++i) {
		if ((cbus_config & 0xf) == FTDI_FTX_CBUS_MUX_GPIO)
			priv->gpio_altfunc &= ~BIT(i);
		cbus_config >>= 4;
	}

out_free:
	kfree(buf);

	return ret;
}

int ftdi_gpio_register(struct usb_interface *interface)
{
	struct ftdi_priv *priv = usb_get_intfdata(interface);

    int result = ftdi_gpio_init_ft232h(interface);

	if (result < 0)
		return result;

	mutex_init(&priv->gpio_lock);

	priv->gc.label = "ftdi-cbus";
	priv->gc.request = ftdi_gpio_request;
	priv->gc.get_direction = ftdi_gpio_direction_get;
	priv->gc.direction_input = ftdi_gpio_direction_input;
	priv->gc.direction_output = ftdi_gpio_direction_output;
	priv->gc.init_valid_mask = ftdi_gpio_init_valid_mask;
	priv->gc.get = ftdi_gpio_get;
	priv->gc.set = ftdi_gpio_set;
	priv->gc.get_multiple = ftdi_gpio_get_multiple;
	priv->gc.set_multiple = ftdi_gpio_set_multiple;
	priv->gc.owner = THIS_MODULE;
	priv->gc.parent = &interface->dev;
	priv->gc.base = -1;
	priv->gc.can_sleep = true;

	result = gpiochip_add_data(&priv->gc, interface);
	if (!result)
		priv->gpio_registered = true;

	return result;


}

void ftdi_gpio_deregister(struct usb_interface *interface)
{
	printk("ftdi: ftdi_gpio_deregister *interface: %p\n", interface);
    struct ftdi_priv *priv = usb_get_intfdata(interface);
	printk("ftdi: ftdi_gpio_deregister *priv: %p\n", priv);

	if (priv->gpio_registered) {
		gpiochip_remove(&priv->gc);
        printk(KERN_INFO "ftdi-gpio: derigistered gpio");
		priv->gpio_registered = false;
	}

	if (priv->gpio_used) {
		/* Exiting CBUS-mode does not reset pin states. */
		ftdi_exit_cbus_mode(interface);
		priv->gpio_used = false;
	}

}
