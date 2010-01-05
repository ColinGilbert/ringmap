#!/usr/local/bin/bash

START_TIME=$(date "+%s")
IDLE_EMPTY_RUN=0
USER_EMPTY_RUN=0
NICE_EMPTY_RUN=0
INTR_EMPTY_RUN=0
SYST_EMPTY_RUN=0
SUM_EMPTY_RUN=0

echo
echo "-----------------------------------------------------"

if [ ${1} = ringmap ]
then 
	echo "CPU Usage for ringmap driver"
	IDLE_EMPTY_RUN=124
	SYST_EMPTY_RUN=3
	USER_EMPTY_RUN=3
	SUM_EMPTY_RUN=130
else 
	if [ ${1} = generic ]
	then 
		echo "CPU Usage for generic driver"
		IDLE_EMPTY_RUN=124
		SYST_EMPTY_RUN=3
		USER_EMPTY_RUN=3
		SUM_EMPTY_RUN=130
	fi
fi


function get_rel() {
	echo $1 $2 | awk '{printf "%.2f", (($1 * 100) / $2)}'
}


function theEnd() {
	sum=0
	t=$(sysctl kern.cp_time)
	CNT_END=${t/kern.*: }

	CNT_RESULT=$(echo ${CNT_END} ${CNT_BEGIN} | awk '{print ($1 - $6), ($2 - $7), ($3 - $8), ($4 - $9), ($5 - $10)}')

	# For debugging
	#echo $CNT_RESULT
	#exit

	user=$(echo $CNT_RESULT ${USER_EMPTY_RUN} | awk '{print $1 - $6}')
	nice=$(echo $CNT_RESULT ${NICE_EMPTY_RUN} | awk '{print $2 - $6}')
	syst=$(echo $CNT_RESULT ${SYST_EMPTY_RUN} | awk '{print $3 - $6}')
	intr=$(echo $CNT_RESULT ${INTR_EMPTY_RUN} | awk '{print $4 - $6}')
	idle=$(echo $CNT_RESULT ${IDLE_EMPTY_RUN} | awk '{print $5 - $6}')

	for i in $CNT_RESULT
	do 
		sum=$(( $sum + $i ))
	done 
	sum=$(( ${sum} - ${SUM_EMPTY_RUN} ))

	idle_rel=$(get_rel ${idle} ${sum})
	user_rel=$(get_rel ${user} ${sum})
	syst_rel=$(get_rel ${syst} ${sum})
	nice_rel=$(get_rel ${nice} ${sum})
	intr_rel=$(get_rel ${intr} ${sum})

	echo "user: "${user_rel} "   idle: "${idle_rel} "   syst: "${syst_rel} "   nice: "${nice_rel} "   intr: "${intr_rel}

	END_TIME=$(date "+%s")

	echo "seconds: "$(( ${END_TIME} - ${START_TIME} ))

	echo "-----------------------------------------------------"
	echo

	exit 0
}


t=$(sysctl kern.cp_time)
CNT_BEGIN=${t/kern.*: }

trap 'theEnd' 14

while true 
do 
	sleep 1
done
