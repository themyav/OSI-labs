obj-m += networkfs.o
networkfs-objs += entrypoint.o tcp_client.o
ccflags-y := -std=gnu11 -Wno-declaration-after-statement

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
