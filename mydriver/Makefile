obj-m += driver.o
 
KDIR = /home/ubuntu/workspace/linux-optee

export ARCH:=arm64
export CROSS_COMPILE:=aarch64-linux-gnu-
 
all:
	make -C $(KDIR)  M=$(shell pwd) modules
 
clean:
	make -C $(KDIR)  M=$(shell pwd) clean
