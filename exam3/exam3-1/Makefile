obj-m :=c_driver.o
KDIR :=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)
default:
	make -C $(KDIR) M=$(PWD) modules
test:usertest
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm usertest