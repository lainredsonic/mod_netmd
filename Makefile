obj-m	:= netmd.o

CC=/usr/bin/gcc
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
#KERNELDIR ?= /usr/src/linux
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)
	$(CC) test_ioc.c -o test_ioc.bin

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c *.bin .tmp_versions

