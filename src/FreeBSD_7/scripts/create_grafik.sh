#!/bin/bash

[ ! -d tmp_graphs ] && mkdir tmp_graphs

X=$1
Y=$2
OUTFILE=$3
TITLE=$4

t=0
indf=0
indt=0
for i in $@
do
	if [  $t -gt 3 ]
	then
		if [ -r $i ]
		then
			files[$indf]=$i
			let indf++
		else 
			if [ $indf -gt 0 ]
			then
				titles[$indt]=$i
				let indt++
			fi
		fi
	fi
	let t++
done

p=0
for i in ${X} ${Y}
do
	line=$(grep "^[:0-9:].*"  ${files[0]} | grep -w ${i})
	positions[$p]=$(echo $line  | awk '{print $1}')
	names[$p]=$i 
	infos[$p]=$(echo $line | cut -d ":" -f2)
	let p++
done

echo "create plot for X,Y = "${names[*]}

p=0
for f in ${files[*]}
do
	plot_files[$p]=tmp_graphs/${p}.dat
	./print_table.sh ${f} ${X} ${Y} ${Y}_dev | grep "^[:0-9:]" | sort > ${plot_files[$p]}
	let p++
done

CMD=$(echo ./doplot.sh ${OUTFILE} ${infos[*]} "${TITLE}" ${plot_files[*]} ${titles[*]})

echo "CMD: "$CMD

eval $CMD

exit 0

