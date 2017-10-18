#ccflags-y = -std=gnu99

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m   := memcache.o
else
# normal makefile
KDIR := /home/iuriidemchenko/Training/linux-4.13/

default:
	$(MAKE) -C $(KDIR) M=$$PWD 
clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
endif
