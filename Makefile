NAME := w1_ds2450
obj-m += src/${NAME}.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
load:
	rmmod ${NAME}
	insmod src/${NAME}.ko
test:
	cat /sys/bus/w1/devices/20-*/status_control
	cat /sys/bus/w1/devices/20-*/convert_all

install:
	cp ${NAME}.ko /lib/modules/$(shell uname -r)/kernel/drivers/w1/slaves/

