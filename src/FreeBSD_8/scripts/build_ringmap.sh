#!/usr/local/bin/bash

# wich system release
uname -r | grep STABLE | grep 8 1>/dev/null
if [ $? -eq 0 ]
then 
	echo ; echo "Building ringmap for FreeBSD-STABLE..."
	echo
	sleep 1
	RINGMAP_BUILD_DIR=../sys/modules/ringmap/
	LIBPCAP_BUILD_DIR=../lib/libpcap/
else 
	echo "Wrong OS"
	exit 1
fi

make_ringmap() {
	cd ${RINGMAP_BUILD_DIR}
	make clean 
	make DEBUG_FLAGS=-g || { echo "Error while compiling driver" ; return 1 ; }
	cd -
}

make_libpcap() {
	cd ${LIBPCAP_BUILD_DIR}
	make clean
	make || { echo "Error by compiling driver" ; return 1 ; }
	cd -
}


###################################
### 		S T A R T           ###
###################################

# Build driver
echo
echo "BUILDING RINGMAP DRIVER"
echo "=================================================================="
sleep 1
make_ringmap
if [ $? -eq 1 ]
then 
	echo
	echo "Stop building! Exit!"
	exit 1;
fi
echo "=================================================================="
echo

# Build libpcap
echo
echo "MAKE LIBPCAP WITH RINGMAP SUPPORT"
echo "=================================================================="
sleep 1
make_libpcap
if [ $? -eq 1 ]
then
	echo
	echo "Stop building! Exit!"
	exit 1;
fi

sync

echo
echo Everything seems to be Ok.
echo For loading ringmap driver and for installing libpcap do:
echo sudo ./set_ringmap.sh
echo

exit 0
