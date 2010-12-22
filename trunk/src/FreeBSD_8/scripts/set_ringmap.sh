#!/bin/sh

# wich system release
uname -r | grep STABLE | grep 8 1>/dev/null
if [ $? -eq 0 ]
then 
	echo "Installing ringmap for FreeBSD-STABLE..."
	echo
	sleep 1
	RINGMAP_BUILD_DIR=../stable_8/sys/modules/ringmap/
	LIBPCAP_BUILD_DIR=../stable_8/lib/libpcap/
else 
	echo "Installing ringmap for FreeBSD-CURRENT..."
	echo
	sleep 1
	RINGMAP_BUILD_DIR=../current/sys/modules/ringmap/
	LIBPCAP_BUILD_DIR=../current/lib/libpcap/
fi


check_module() {
	kldstat | grep ringmap >/dev/null
	if [ $? -eq 0 ]
	then 
		echo The old ringmap module is loaded.
		echo It will be first unloaded... && sleep 1
		kldunload if_ringmap.ko
		sleep 1
	fi 
}

install_and_load_ringmap() {
	cd ${RINGMAP_BUILD_DIR}
	make install || { echo "Error while compiling driver" ; return 1 ; }
	kldload if_ringmap
	cd -
}

install_libpcap() {
	cd ${LIBPCAP_BUILD_DIR}
	make install || { echo "Error by compiling driver" ; return 1 ; }
	cd -
}

echo
echo "===> Check the module"
check_module || { Error! ; exit 1 ; }

echo
echo "===> kld-Load ringmap-driver"
install_and_load_ringmap || { echo "Error by installing ringmap" ; exit 1 ; }
echo

sleep 1

echo
echo "===> Install ringmap-libpcap:" && sleep 1
echo
install_libpcap || { echo "Error by installing libpcap" ; exit 1 ; }
echo

sync

echo "Before using ringmap set the network interface in monitoring mode"
echo
exit 0
