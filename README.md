FTDI general driver
===================

Description
-----------

This is an attempt to create a ftdi kernel module which allows switching
between serial mode and i2c

Dependencies
-----------

- linux-header
- make
- gcc
- binutils

Build
-----

To build this kernel module use the makefile included.

Build with all cores:
```(shell)
$ make -j $(nproc)
```

Install  on current system:
```(shell)
$ make modules_install
```

Generate compile_commands.json:
```(shell)
$ make compile_commands.json
```


Usage
-----

Ether load the module with:
```(shell)
$ modprobe ftdi
```
or use `depmods` to enable hotplugging.

To change the mode write to the `ftdi_mode` file in the sysfs from the driver:

Get possible modes:
```(shell)
$ cat /sys/bus/usb/drivers/ftdi_driver/<usb device id >/ftdi_mode
I2C     GPIO    [SERIAL]        SPI
```
Note: `[]` indicates the current Modes.

Set mode (eg. `I2C`):
```(shell)
$ echo "I2C" > /sys/bus/usb/drivers/ftdi_driver/<usb device id >/ftdi_mode
```

References
----------

- [Linux kernel ftdi-sio](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/usb/serial/ftdi_sio.c)
- [Possible I2C implementation from krinikum](https://github.com/krinkinmu/bootlin)
- [Another implementation of multiple ftdi modes from bm16ton](https://github.com/bm16ton/ft2232-mpsse-i2c-spi-kern-drivers)
