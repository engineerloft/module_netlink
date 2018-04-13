KDIR := /lib/modules/$(shell uname -r)/build

obj-m := mod_netlink.o mod_nl_ts.o
mod_netlink-objs := module_netlink.o nl_ts_queue.o

mod_nl_ts-objs := nl_ts_module.o nl_ts_queue.o

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules 

clean:
	rm -rf *.o *.ko *.mod.* *.cmd .module* modules* Module* .*.cmd .tmp*
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
