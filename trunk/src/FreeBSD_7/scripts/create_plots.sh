#!/bin/bash

W_ring=$1
W_generic=$2
DST_DIR=$3

[ ! -d ${DST_DIR} ] && mkdir ${DST_DIR}

sizes=$(for i in *_testsparam ; do echo ${i/_*/} ; done | sort -n)

comm_params=(missed_perc sysload cap_time)
ringmap_params=(missed_perc missed sysload intrnum_rate pkt_intr)

for count in 5000000 10000000 15000000
do 

	for size in ${sizes}
	do
		fnr=${W_ring}/${size}_${count}
		fng=${W_generic}/${size}_${count}

		fnrt=$(grep "^Driver:" ${fnr} | awk '{print $2}')
		fngt=$(grep "^Driver:" ${fng} | awk '{print $2}')

		# Do plots with both ringmap and generic
		for x in mbs pps
		do
			for y in ${comm_params[*]}
			do
				OUTF=${DST_DIR}/${size}_${count}_ringmap_generic_${x}_${y}.eps
				TITLE="Ringmap\ and\ Generic\ Comparision"
				echo "File : "$OUTF
				echo "Titles : " $fnrt  $fngt
				./create_grafik.sh $x $y $OUTF "${TITLE}" $fnr $fng "${fnrt}" "${fngt}"
				echo
			done
		done

		# Do some ringmap plots
		for x in mbs pps pkt_intr intrnum_rate pkt_rate
		do
			for y in ${ringmap_params[*]}
			do
				[ $x = $y ] && continue
				OUTF=${DST_DIR}/${size}_${count}_ringmap_${x}_${y}.eps
				TITLE="Ringmap\ Driver"
				echo "File : "${OUTF}
				echo "Title : " ${fnrt}
				./create_grafik.sh "${x}" ${y} "${OUTF}" "${TITLE}" ${fnr} "${fnrt}" 
				echo
			done
		done

		echo "--------------------------------------------------------------------"
		echo
	done
done
