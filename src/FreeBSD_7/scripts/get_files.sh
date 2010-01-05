#!/bin/bash

# Input params 
WHERE=$1
LENGTH=$2
COUNT=$3

# Vars for script 
FILE_PARAMS=${LENGTH}"_testsparam"

# FUNCS
function get_fname() {
	gap=$1
	sav=$2
	try=$3
	echo ${WHERE}"/"${LENGTH}"/"${try}"/"${LENGTH}"_"${gap}"_"${sav}"_"${COUNT}
}

if [ ! -r ${FILE_PARAMS} ] 
then 
	echo "File withe tests params is missing: "${FILE_PARAMS}
	exit 1
fi

while read line
do
	g=$(echo $line | awk '{print $1}')
	s=$(echo $line | awk '{print $2}')

	for try in 1 2 3 4 5 
	do 
		echo $(get_fname ${g} ${s} ${try})
	done

done < ${FILE_PARAMS}

exit 0
