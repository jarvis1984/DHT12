KERNELDIR ?= /root/workspace/lichee/linux-3.4/
PWD := $(shell pwd)
obj-m := dht12.o
    
all:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(KERNELDIR) M=$(PWD) modules
    
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
ifneq (, $(wildcard test))
	rm test
endif

test:
	arm-linux-gnueabihf-gcc test.c -o test

doxygen:
	doxygen Doxygen.conf