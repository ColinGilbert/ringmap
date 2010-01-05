#!/bin/bash

# interfaces
ifaces=(eth2 eth3)

# F U N C T I O N S #########################################################

function pgset() {
    local result

    echo $1 > $PGDEV

    result=`cat $PGDEV | fgrep "Result: OK:"`
    if [ "$result" = "" ]; then
         cat $PGDEV | fgrep Result:
    fi
}

function pg() {
    echo inject > $PGDEV
    cat $PGDEV
}

function clear_threads() {
	ls /proc/net/pktgen/ | grep kpktgen | \
	while read line  
	do 
		PGDEV=/proc/net/pktgen/$line
		echo "Removing all devices from "$line
		pgset "rem_device_all" 
	done

}

function setuppktgen() {
	PKTGEN_THREAD=$1
	NIC_DEVICE=$2
	PGDEV=/proc/net/pktgen/$PKTGEN_THREAD

	echo "Adding "$NIC_DEVICE

	pgset "add_device "$NIC_DEVICE
	pgset "max_before_softirq "$MAX_BEFORE_SOFTIRQ

	PGDEV=/proc/net/pktgen/$NIC_DEVICE
	echo "Configuring $PGDEV"

	# Set addresses 
	pgset "dst_mac 00:04:23:8d:5a:0a"
	pgset "dst 12.0.0.10"
	pgset "udp_src_min 1122"
	pgset "udp_src_max 1122"
	pgset "udp_dst_min 2211"
	pgset "udp_dst_max 2211"

	pgset "count "$COUNT
	pgset "clone_skb "$CLONE_SKB
	pgset "pkt_size "$PKT_SIZE
	pgset "delay "$DELAY
	#pgset "tos "$TOS
}

function generate_and_send(){
	PGDEV=/proc/net/pktgen/pgctrl

	echo "Running... ctrl^C to stop"
	pgset "start" 
}

function print_results(){
	for NIC_DEVICE in ${ifaces[*]} ; do
	echo
	echo "R E S U L T S   F O R  "${NIC_DEVICE}":"
	echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	echo "pkts count = "$COUNT", pkts size = "$PKT_SIZE", delay = "$DELAY
	echo 
	cat /proc/net/pktgen/$NIC_DEVICE
	echo ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	echo 
	done
}

function check_and_load_pktgen_module()
{
	lsmod | grep pktgen 1>/dev/null
	if [ $? -ne 0 ]
	then 
		echo "pktgen module isn't loaded"
		echo "try modprobe pktgen..."
		modprobe pktgen
		if [ $? -ne 0 ]
		then 
			echo "Error. pktgen module can't be loaded. Exit!"
			exit
		fi 
	fi
	echo "pktgen loaded!"
}

function set_ifaces() {
	t=0
	for i in ${ifaces[*]}
	do
		echo "set up iface "$i
		setuppktgen kpktgend_$t $i
		let t++
	done

}

################################################
###          	    S T A R T                ###
################################################

# Check input parameters
if [ $# -ne 3 ]
then 
	echo "Wrong number of parameters"
	echo "Usage: sudo "$0"  count size delay"
	exit 1
fi

MAX_BEFORE_SOFTIRQ=10000
CLONE_SKB=10000

# I want to mark our pkts
#TOS=00

COUNT=$1
PKT_SIZE=$2
DELAY=$3

# insert pktgen module if missing
check_and_load_pktgen_module

# clear threads
clear_threads 

# setup linux pktgen
set_ifaces

# Start pkt generation and sending
generate_and_send
echo "Done"

# Print Results
print_results
