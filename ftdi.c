#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/string.h>

#define FTDI_VENDOR 0x0403
#define FTDI_FT232H 0x6014

static struct usb_device_id ftdi_table[] =
{
    { USB_DEVICE(FTDI_VENDOR, FTDI_FT232H) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, ftdi_table);

static const char *ftdi_driver_mode_names[] =
{
    "I2C",
    "GPIO",
    "SERIAL",
    "SPI",
};

enum ftdi_driver_mode_types {
    FTDI_MODE_I2C = 0,
    FTDI_MODE_GPIO,
    FTDI_MODE_SERIAL,
    FTDI_MODE_SPI,
    FTDI_MODE_ENUM_END,
};

struct ftdi_priv {
	struct usb_device *	usb_dev;
    enum ftdi_driver_mode_types mode;
};


/* ----------------------------------------------------------------------------
 *                             FTDI hardware driver
 * ----------------------------------------------------------------------------
 */


/* ----------------------------------------------------------------------------
 *                             USB driver stuff
 * ----------------------------------------------------------------------------
 */

/* ---------- userspace communication stuff ---------- */

static ssize_t set_ftdi_mode(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
    struct usb_interface *interface = to_usb_interface(dev);
    struct ftdi_priv *ftdi_dev = usb_get_intfdata(interface);

    // test for NULL
    if (count == 0  || buf == NULL)
        return -EINVAL;

    // remote potential new line
    char *local_buffer = kzalloc(count + 1,GFP_KERNEL);
    memcpy(local_buffer, buf, count);
    local_buffer[strcspn(buf, "\n")] = 0;

    // iterate over mode array
    for (int i = 0; i < FTDI_MODE_ENUM_END; i++) {
	    if (!strncmp(ftdi_driver_mode_names[i], local_buffer, count)) {
		    ftdi_dev->mode = i;
		    goto ftdi_found;
	    }
    }
    printk(KERN_WARNING "ftdi: could not set mode \"%s\"(%s) for device\n",buf,local_buffer);
    kfree(local_buffer);
    return -EINVAL;

ftdi_found:

    printk(KERN_INFO "ftdi: set mode \"%s\"(%s) for device\n",buf,local_buffer);
    kfree(local_buffer);

    return count;
}


static ssize_t show_ftdi_mode(struct device *dev, struct device_attribute *attr,
			char *buf)
{


    for (int i = 0; i < FTDI_MODE_ENUM_END; i++) {
        strcat(buf,ftdi_driver_mode_names[i]);
        strcat(buf,"\n");
    }

    return strlen(buf)+1;
}

static DEVICE_ATTR(ftdi_mode, 0660, show_ftdi_mode, set_ftdi_mode);

/* ---------- Init stuff ---------- */
static int ftdi_probe(struct usb_interface *interface, const struct usb_device_id *id)
{

    // per device struct
    struct ftdi_priv* dev = kzalloc(sizeof(struct ftdi_priv), GFP_KERNEL);

    // get usb dev for sysfs file in driver
    struct usb_device *usbdev = interface_to_usbdev(interface);

    dev->usb_dev  = usbdev;
    usb_set_intfdata(interface, dev);

    int ret = device_create_file(&interface->dev, &dev_attr_ftdi_mode);
    if (ret < 0)
	    printk(KERN_ERR "device_create_file error\n");

    printk("ftdi: (%04X:%04X) created\n", id->idVendor,
                                id->idProduct);
    return 0;
}

static void ftdi_disconnect(struct usb_interface *interface)
{
    struct ftdi_priv *dev = usb_get_intfdata(interface);
     usb_set_intfdata(interface, NULL);

    // get usb dev for sysfs file in driver
    struct usb_device *usbdev = interface_to_usbdev(interface);
    device_remove_file(&interface->dev, &dev_attr_ftdi_mode);



    kfree(dev);
    printk("ftdi: drive removed\n");
}

static struct usb_driver ftdi_driver =
{
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
