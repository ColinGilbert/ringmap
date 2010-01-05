#!/bin/bash

if [ $# -ne 4 ]
then
	echo "Wrong number of parameter"
	echo "Usage: "${0}" sender capturer driver info"
	exit 1
fi

SENDER=$1		# login@host - where is scripts for generating and sending pkts
CAPTURER=$2		# login@host - system where trafic will captured
TYPE_OF_DRIVER=$3	# ringmap or generic
OPTIONS=$4		# some information

# Path to ringmap-driver and libpcap source on capturing system
PATH_TO_SRC="/home/alexandre/alexandre-da/src/70/"
SENDER_SCRIPTS_PATH="scripts/"
SENDER_STATS_PATH=${SENDER_SCRIPTS_PATH}
CAPTURER_SCRIPTS_PATH=${PATH_TO_SRC}"/scripts/"
CAPTURER_STATS_PATH=${PATH_TO_SRC}"/userspace/"

number_of_send_pkts=(5000000 10000000 15000000)
pkt_length=$(for i in *_testsparam ; do echo ${i/_*/} ; done | sort -n)

# Where are directories with resunt placed on all systems
DIR="tests/"${TYPE_OF_DRIVER}"_"${OPTIONS}

CAPT_IFACE=ringmap0

[ ${TYPE_OF_DRIVER} = generic ] && CAPT_IFACE=em0

echo "sender: "$SENDER", capturer: "$CAPTURER", TYPE_OF_DRIVER: "$TYPE_OF_DRIVER", OPTIONS: "$OPTIONS", CAPT_DEVICE: "${CAPT_IFACE}
echo "All statistics will placed in "$DIR


function reload_driver() {
	echo "Reload "$TYPE_OF_DRIVER
	ssh $CAPTURER "cd ${CAPTURER_SCRIPTS_PATH} ; ./reload_driver $1"
}

function check_dirs() {
	[ ! -d tests ] && mkdir tests
	[ ! -d $DIR ] && mkdir ${DIR}

	ssh $SENDER "cd ${SENDER_STATS_PATH} ; [ ! -d tests ] && mkdir tests ; [ ! -d $DIR ] && mkdir ${DIR}"
	ssh $CAPTURER "cd ${CAPTURER_STATS_PATH} ; [ ! -d tests ] && mkdir tests ; [ ! -d $DIR ] && mkdir ${DIR}"
}

function set_capturing_driver() {
	echo "Login to capturing PC and install "${TYPE_OF_DRIVER}" driver"
	ssh $CAPTURER "cd ${CAPTURER_SCRIPTS_PATH} ; ./stop_capturing ${TYPE_OF_DRIVER} ; ./stop_cpusage ; ./${TYPE_OF_DRIVER}_build_and_load"
}

function start_capturing() {
	echo "Start libpcap-application on the capturing PC"
	ssh ${CAPTURER} "cd ${PATH_TO_SRC}"/userspace/"; ifconfig ${CAPT_IFACE} 13.0.0.2 promisc up ; sleep 5 ; ./${TYPE_OF_DRIVER} -1 -1 2 ${CAPT_IFACE} > ${1} &"
}

function stop_capturing() {
	echo "Stop capturing process"
	ssh ${CAPTURER} "cd ${CAPTURER_SCRIPTS_PATH} ; ./stop_capturing ${TYPE_OF_DRIVER}"
}

function generate_pkts_and_send() {
	echo ${SENDER}" - Generate "${1}" pkts. Each pkt "${2}" bytes. Gap: "${3}  
	ssh ${SENDER} "cd scripts ; ./pktgen.sh ${1} ${2} ${3} ${4} ${TYPE_OF_DRIVER} | ./get_rate > ${5}"
}

function get_results() {
	SRC=$1
	DST=$2
	TMP_FILE="test.tmp"

	# flush file system buffers by sender and capturer.
	ssh ${SENDER} "sync"
	ssh ${CAPTURER} "sync"


	echo "Transfer "${SENDER}:${SENDER_STATS_PATH}"/"${SRC}" to "${DST}
	scp ${SENDER}:${SENDER_STATS_PATH}"/"${SRC} ${TMP_FILE}

	date >> ${DST}
	echo "SENDING INFORMATION: " >> ${DST}
	cat ${TMP_FILE} >> ${DST}
	echo " " >> ${DST}
	echo "---------------------" >> ${DST}
	echo " " >> ${DST}
	echo "CAPTURING INFORMATION:" >> ${DST}
	echo " " >> ${DST}

	echo "Transfer "${CAPTURER}:${CAPTURER_STATS_PATH}"/"${SRC}" to "${DST}
	scp ${CAPTURER}:${CAPTURER_STATS_PATH}"/"${SRC} ${TMP_FILE}
	cat ${TMP_FILE} >> ${DST}

	echo 
	echo "CPU Usage:" >> ${DST}
	scp ${CAPTURER}:${CAPTURER_SCRIPTS_PATH}"/"cpu.tmp ${TMP_FILE}
	cat ${TMP_FILE} >> ${DST}

	echo " " >> ${DST}
	echo "+++++++++++++   The END   +++++++++++++" >>${DST}
}

function doit() {
	PKT_LENGTH=$1
	PKT_GAP=$2
	s=$3

	[ ! -d ${DIR}"/"${PKT_LENGTH} ] && mkdir $DIR"/"${PKT_LENGTH}

	for i in 1 2 3 4 5
	do
		[ ! -d ${DIR}"/"${PKT_LENGTH}"/"${i} ] && mkdir ${DIR}"/"${PKT_LENGTH}"/"${i}
		for n in ${number_of_send_pkts[*]}
		do
			fn=${PKT_LENGTH}"_"${PKT_GAP}"_"${s}"_"${n}
			OUR_FILE=${DIR}"/"${PKT_LENGTH}"/"${i}"/"${fn}
			REMOTE_FILE=${DIR}"/"${fn}
			
			date

			start_capturing ${REMOTE_FILE}

			sleep 1

			generate_pkts_and_send ${n} ${PKT_LENGTH} ${PKT_GAP} ${s} ${REMOTE_FILE}

			sleep 4

			stop_capturing

			sleep 1

			get_results ${REMOTE_FILE} ${OUR_FILE}

			reload_driver $TYPE_OF_DRIVER

			sleep 1

			echo 
		done
	done
}

function begin() {

	for pl in ${pkt_length[*]}
	do 
		ind=0
		while read line
		do
			g[$ind]=$(echo $line | awk '{print $1}')
			s[$ind]=$(echo $line | awk '{print $2}')
			let ind++
		done < ${pl}_testsparam
		
		# echo " i = " $ind  
		ta=0
		while [ $ta -lt $ind ]
		do
			# echo "ta = " $ta "   ind = "${ind}
			# echo "Do tests with  gap - s:  " ${g[$ta]} " - " ${s[$ta]}
			
			doit ${pl} ${g[$ta]} ${s[$ta]}
			echo "Pkt length: "${pl}"     Pkt gap: "${g[${ta}]}"   SKB: "${s[${ta}]} "   -     done."
			echo 
			echo "======================================================================="
			echo 

			let ta++
		done 

	done 
}

#########################################################
###			S T A R T		      ###
#########################################################

echo
echo "Do tests..."
check_dirs
set_capturing_driver 
sleep 2
begin
echo 
echo "All Tests Done!"
