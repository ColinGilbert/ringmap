#!/bin/sh

LIBPCAP_BUILD_DIR=/usr/src/lib/libpcap/

check_module() {
	kldstat | grep ringmap >/dev/null
	if [ $? -eq 0 ]
	then 
		echo The ringmap module is loaded.
		echo It will be first unloaded... && sleep 1
		kldunload if_ringmap.ko || { echo "Can not unload the module. Probably root permissions are needed." ; return 1 ; }
		sleep 1
	fi 
}

install_libpcap() {
	cd ${LIBPCAP_BUILD_DIR}
	make clean 
	make || { echo "can not build libpcap" ; return 1 ; }
	make install || { echo "Error by installing libpcap" ; return 1 ; }
	cd -
}

echo
echo "===> Check the module"
check_module || { echo "Error!" ; exit 1 ; }

echo
echo "===> Install libpcap:" && sleep 1
echo
install_libpcap || { echo "Error by installing libpcap" ; exit 1 ; }
echo

echo "If you want to use network interface with generic driver do kldload <module name>"
echo

exit 0;
