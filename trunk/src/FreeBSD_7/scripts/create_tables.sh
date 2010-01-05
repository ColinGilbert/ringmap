#!/bin/bash

W_ring=$1
W_generic=$2
DST_DIR=$3

first_col=(pps mbs)

columns=(missed missed_perc sysload pkt_rate)
ringmap_cols=(pps mbs missed_perc sysload intrnum cap_time intrnum_rate pkt_intr bytes_intr bytes_rate pkt_rate)

[ ! -d ${DST_DIR} ] && mkdir ${DST_DIR}

for count in 5000000 10000000 15000000
do 
		echo "Count :"${count}
		echo "ringmap" > ${count}_ring
		echo "generic" > ${count}_generic

		files=$(ls ${W_ring}/*_${count} | sed -e 's/.*\///g' | grep "^[:0-9:]" | sort -n)

		for file in ${files}
		do
			fnr=${W_ring}/${file}
			fng=${W_generic}/${file}
			size=$(echo ${file} | sed -e "s/_.*//")

			./print_table.sh ${fnr} ${first_col[*]} ${columns[*]} |\
			while read line 
			do 
				echo $line | grep "^[:0-9:]" > /dev/null && \
					printf "%-13s" ${size} ${line} >> ${count}_ring || \
					printf "%-13s" "size" $line >> ${count}_ring 

				printf "\n" >> ${count}_ring
			done 
			printf "\n" >> ${count}_ring
			
			./print_table.sh ${fng} ${columns[*]} >> ${count}_generic
			printf "\n" >> ${count}_generic

			RINGMAP_TABLE_FILE=${DST_DIR}/${size}_${count}_ringmap_table
			echo "Driver: ringmap" > ${RINGMAP_TABLE_FILE}
			echo "Test with "${count} " packets. Packet size: "${size} >> ${RINGMAP_TABLE_FILE} 
			for msg in ${ringmap_cols[*]}
			do 
				echo ${msg}" - " $(cat ${fnr} | grep ":" | grep -w ${msg} | cut -d ":" -f2 -) | sed -e "s/\\\//g"\
				>> ${RINGMAP_TABLE_FILE}
			done 
			echo >> ${RINGMAP_TABLE_FILE}
				
		./print_table.sh ${fnr} ${ringmap_cols[*]} >> $RINGMAP_TABLE_FILE

		done 	

		DST_TABLE_FILE=$DST_DIR/${count}_ringmap_generic_table

		echo "Test with "${count} " packets" > ${DST_TABLE_FILE}
		
		paste ${count}_ring ${count}_generic >> ${DST_TABLE_FILE}
		rm -f ${count}_ring ${count}_generic 
done 
exit 0
