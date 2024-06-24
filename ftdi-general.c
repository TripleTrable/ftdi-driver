#include "ftdi.h"
#include "linux/usb.h"
int ftdi_read_eeprom(struct usb_interface *interface, void *dst, u16 addr,
				u16 nbytes)
{
    struct usb_device *dev = interface_to_usbdev(interface);
	int read = 0;

	if (addr % 2 != 0)
		return -EINVAL;
	if (nbytes % 2 != 0)
		return -EINVAL;

	/* Read EEPROM two bytes at a time */
	while (read < nbytes) {
		int rv;

		rv = usb_control_msg(dev,
				     usb_rcvctrlpipe(dev, 0),
				     FTDI_SIO_READ_EEPROM_REQUEST,
				     FTDI_SIO_READ_EEPROM_REQUEST_TYPE,
				     0, (addr + read) / 2, dst + read, 2,
				     WDR_TIMEOUT);
		if (rv < 2) {
			if (rv >= 0)
				return -EIO;
			else
				return rv;
		}

		read += rv;
	}

	return 0;
}

