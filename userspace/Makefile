all:
	make -C ../lib/libnl
	gcc  -o userspace_netlink.run userspace_netlink.c \
	-I../lib/libnl/include -L../lib/libnl/lib/.libs \
	-l:libnl-3.a -l:libnl-genl-3.a -lpthread -lm
	
clean:
	make -C ../lib/libnl clean
	rm userspace_netlink.run
