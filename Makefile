userInput: userInput.c
	gcc-5 `pkg-config --cflags gtk+-3.0` -o userInput userInput.c `pkg-config --libs gtk+-3.0`

ifneq ($(KERNELRELEASE),) 
obj-m := keylogger.o
else 

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 

PWD := $(shell pwd)

default: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules  
endif 
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean


