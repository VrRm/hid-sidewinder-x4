KERNELRELEASE ?= $(shell uname -r)
KDIR := /lib/modules/$(KERNELRELEASE)
KSDIR := $(KDIR)/build
PWD := $(shell pwd)

obj-$(CONFIG_HID_MICROSOFT) += hid-microsoft.o

modules:
	$(MAKE) -C "$(KSDIR)" M="$(PWD)" modules

modules_install: modules
	$(MAKE) -C "$(KSDIR)" M="$(PWD)" modules_install

clean:
	$(MAKE) -C "$(KSDIR)" M="$(PWD)" clean

help:
	$(MAKE) -C "$(KSDIR)" M="$(PWD)" help

reload:
	rmmod hid-microsoft --force || true
	insmod hid-microsoft.ko

# for development only
forced_version:
	$(MAKE) -C "$(HOME)/src/linux/hid-build" M="$(PWD)" modules
