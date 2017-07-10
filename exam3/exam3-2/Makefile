CONFIG_MODULE_SIG=n
obj-m :=blk_dev.o
KDIR :=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)
default:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean