obj-m += kfetch_mod_311512040.o 
PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


chardevclient:
	gcc -Wall chardevclient.c -o chardevclient
