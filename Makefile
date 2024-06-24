obj-m += ftdi.o

ftdi-objs += ftdi-base.o ftdi-general.o ftdi-gpio.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

compile_commands.json:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) compile_commands.json

modules_install:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules_install
