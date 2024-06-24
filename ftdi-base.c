#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/usb.h>

#include "ftdi.h"

#define FTDI_VENDOR 0x0403
#define FTDI_FT232H 0x6014

static struct usb_device_id ftdi_table[] = {
	{ USB_DEVICE(FTDI_VENDOR, FTDI_FT232H) },
	{} /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, ftdi_table);

static const char *ftdi_driver_mode_names[] = {
	"I2C",
	"GPIO",
	"SERIAL",
	"SPI",
};



/* ----------------------------------------------------------------------------
 *                             USB driver stuff
 * ----------------------------------------------------------------------------
 */

static inline void ftdi_disable_mode(struct usb_interface *interface,
				     enum ftdi_driver_mode_types mode)
{
	switch (mode) {
	case FTDI_MODE_I2C:
		break;
	case FTDI_MODE_GPIO:
		ftdi_gpio_deregister(interface);
		break;
	case FTDI_MODE_SERIAL:
		break;
	case FTDI_MODE_SPI:
		break;
	}
}

static inline void ftdi_enable_mode(struct usb_interface *interface,
				     enum ftdi_driver_mode_types mode)
{
	switch (mode) {
	case FTDI_MODE_I2C:
		break;
	case FTDI_MODE_GPIO:
		ftdi_gpio_register(interface);
		break;
	case FTDI_MODE_SERIAL:
		break;
	case FTDI_MODE_SPI:
		break;
	}
}
/* ---------- userspace communication stuff ---------- */

static ssize_t set_ftdi_mode(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct usb_interface *interface = to_usb_interface(dev);
	struct ftdi_priv *ftdi_dev = usb_get_intfdata(interface);
	enum ftdi_driver_mode_types prev_mode = ftdi_dev->mode;

	// test for NULL
	if (count == 0 || buf == NULL)
		return -EINVAL;

	// remote potential new line
	char *local_buffer = kzalloc(count + 1, GFP_KERNEL);
	memcpy(local_buffer, buf, count);
	local_buffer[strcspn(buf, "\n")] = 0;

	// iterate over mode array
	for (int i = 0; i < FTDI_MODE_ENUM_END; i++) {
		if (!strncmp(ftdi_driver_mode_names[i], local_buffer, count)) {
			ftdi_dev->mode = i;
			goto ftdi_found;
		}
	}
	printk(KERN_WARNING "ftdi: could not set mode \"%s\"(%s) for device\n",
	       buf, local_buffer);
	kfree(local_buffer);
	return -EINVAL;

ftdi_found:

    ftdi_disable_mode(interface, prev_mode);
    ftdi_enable_mode(interface, ftdi_dev->mode);

	printk(KERN_INFO "ftdi: set mode \"%s\"(%s) for device\n", buf,
	       local_buffer);
	kfree(local_buffer);

	return count;
}

static ssize_t show_ftdi_mode(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct usb_interface *interface = to_usb_interface(dev);
	struct ftdi_priv *ftdi_dev = usb_get_intfdata(interface);

    strcat(buf, ftdi_driver_mode_names[0]);
	for (int i = 1; i < FTDI_MODE_ENUM_END; i++) {
		strcat(buf, "\t");
		if (i == ftdi_dev->mode)
			strcat(buf, "[");

		strcat(buf, ftdi_driver_mode_names[i]);

		if (i == ftdi_dev->mode)
			strcat(buf, "]");

	}

	return strlen(buf);
}

static DEVICE_ATTR(ftdi_mode, 0660, show_ftdi_mode, set_ftdi_mode);

/* ---------- Init stuff ---------- */
static int ftdi_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	// per device struct
	struct ftdi_priv *ftdi_dev =
		kzalloc(sizeof(struct ftdi_priv), GFP_KERNEL);

	ftdi_dev->mode = FTDI_MODE_GPIO;

	// get usb dev for sysfs file in driver
	struct usb_device *usbdev = interface_to_usbdev(interface);

	// set priv data
	ftdi_dev->usb_dev = usbdev;
	usb_set_intfdata(interface, ftdi_dev);

	int ret = device_create_file(&interface->dev, &dev_attr_ftdi_mode);
	if (ret < 0)
		printk(KERN_ERR "device_create_file error\n");

    // initialize gpio mode as default until serial is done
    // when serial is implemented this should be the default to match the
    // existing ftdi_sio driver
    ret = ftdi_gpio_register(interface);
    if (ret < 0)
		printk(KERN_ERR "Could not initialize gpio\n");

	printk("ftdi: (%04X:%04X) created\n", id->idVendor, id->idProduct);
	return 0;
}

static void ftdi_disconnect(struct usb_interface *interface)
{
	struct ftdi_priv *dev = usb_get_intfdata(interface);

	printk("ftdi: driver mode is %d\n", dev->mode);
	if (dev->mode == FTDI_MODE_GPIO)
		ftdi_gpio_deregister(interface);


	// get usb dev for sysfs file in driver
	device_remove_file(&interface->dev, &dev_attr_ftdi_mode);

	kfree(dev);
	printk("ftdi: driver removed\n");
}


/* ----------------------------------------------------------------------------
 *                      USB driver module boilerplate
 * ----------------------------------------------------------------------------
 */


static struct usb_driver ftdi_driver = {
	.name = "ftdi_driver",
	.id_table = ftdi_table,
	.probe = ftdi_probe,
	.disconnect = ftdi_disconnect,
};

static int __init ftdi_init(void)
{
	printk("ftdi: init\n");
	return usb_register(&ftdi_driver);
}

static void __exit ftdi_exit(void)
{
	printk("ftdi: exit\n");
	return usb_deregister(&ftdi_driver);
}

module_init(ftdi_init);
module_exit(ftdi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lars Niesen <lars.niesen@mailbox.org>");
MODULE_DESCRIPTION("FTDI general driver");
