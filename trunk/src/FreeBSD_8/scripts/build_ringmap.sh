#!/usr/local/bin/bash

checkOS() {
	uname -r | grep STABLE | grep 8 1>/dev/null
	if [ $? -eq 0 ]
	then 
		# Check for sorces: Kernel
		if [ -d /usr/src/sys ] 
		then 
			echo "Kernel source is present - Ok"
		else 
			echo "Error! Kernel source is missing! Please, download kernel source in /usr/src/"
			exit 1;
		fi

		# Check versions
		P=$(cat /usr/src/sys/sys/param.h | grep define | grep __FreeBSD_version | awk '{print $3}')
		T=$(sysctl kern.osreldate | awk '{print $2}')
		if [ ${P} -eq ${T} ] 
		then 
			echo "Systemversion -  Ok"
		else 
			echo "Error! Version mismatch! Installed OS does not relate to the source in /usr/src/sys"
			exit 1
		fi 
	else 
		echo "Wrong OS"
		exit 1
	fi
}

make_ringmap() {
	# Check variables in /etc/make.conf
	cat /etc/make.conf | grep EM_RINGMAP 1>/dev/null
	if [ $? -eq 0 ]
	then 
		echo ; echo "make.conf - checked" ; echo
	else 
		echo "Set EM_RINGMAP variable in /etc/make.conf"
		return 1
	fi 

	cd ${RINGMAP_BUILD_DIR}

	make clean 

	#  For debugging use: make DEBUG_FLAGS=-g
	make || { echo "Error while compiling driver" ; return 1 ; }
	cd -
}

make_libpcap() {
	# Check variables in /etc/make.conf
	cat /etc/make.conf | grep LIBPCAP_RINGMAP 1>/dev/null
	if [ $? -eq 0 ]
	then 
		echo ; echo "make.conf - checked" ; echo
	else 
		echo "Set LIBPCAP_RINGMAP variable in /etc/make.conf"
		return 1
	fi 


	cd ${LIBPCAP_BUILD_DIR}
	make clean
	make || { echo "Error by compiling driver" ; return 1 ; }
	cd -
}


###################################
### 		S T A R T           ###
###################################

RINGMAP_BUILD_DIR=../sys/modules/ringmap/
LIBPCAP_BUILD_DIR=../lib/libpcap/

# Check OS 
checkOS

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
