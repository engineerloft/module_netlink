#! /bin/bash

git submodule init
git submodule update
cdir=$(pwd)
cd ../lib/libnl
./autogen.sh
./configure
cd ${cdir}
echo -e "all:" > Makefile
echo -e "\tmake -C ../lib/libnl" >> Makefile
echo -e "\tgcc  -o userspace_netlink.run userspace_netlink.c -I../lib/libnl/include -L../lib/libnl/lib/.libs -l:libnl-3.a -l:libnl-genl-3.a -lpthread -lm" >> Makefile
echo -e ""	>> Makefile
echo -e "clean:" >> Makefile
echo -e "\tmake -C ../lib/libnl clean" >> Makefile
echo -e "\trm userspace_netlink.run" >> Makefile
