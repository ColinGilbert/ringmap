#!/bin/bash

FILE_NAME=$1
DIST=-15

p=0
for i in $@
do

	if [ $p = 0 ]
	then 
		let p++
		continue
	fi

	tmp=$(grep ":" ${FILE_NAME} | grep -w ${i} | awk '{print $1}')
	echo ${tmp} | grep "[:0-9:]" > /dev/null && positions[$p]=$tmp && names[$p]=$i && let p++ 

done

for n in ${names[*]} 
do
	printf "%${DIST}s" $n
done
echo

while read line 
do 
	echo $line | grep -v ":" | grep "^[:0-9:]" > /dev/null && l=${line} || continue
	for i in ${positions[*]}
	do
		val=$(echo $l $i | awk '{print $($NF)}')
		printf "%${DIST}s" $val
	done
	printf "\n"
done < ${FILE_NAME}
