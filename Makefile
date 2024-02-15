
obj-m := message_slot.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: message_sender.o message_reader.o
	$(MAKE) -C $(KDIR) M=$(PWD) CC=x86_64-linux-gnu-gcc-12 modules

message_sender.o:
	gcc $(PWD)/message_sender.c -o message_sender

message_reader.o:
	gcc $(PWD)/message_reader.c -o message_reader

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm $(PWD)/message_sender
	rm $(PWD)/message_reader
