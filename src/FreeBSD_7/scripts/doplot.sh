#!/bin/bash

{
        echo "set terminal postscript eps color enhanced"
        echo "set output '${1}'"
        echo "set xlabel '${2}'"
        echo "set ylabel '${3}'"
        echo "set title '${4}'"
        # echo "set xrange [ '${5}' : '${6}' ]"
        # echo "set yrange [ '${7}' : '${8}' ]"
        echo "set key left top Left reverse"

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
		pt="plot"
		indt=0
		for i in ${files[*]}
		do
			if [ $p = 0 ]
			then
				t=" '${i}' with linesp title '${titles[$indt]}' , '${i}' notitle with errorbars"
			else 
				t=" , '${i}' with linesp title '${titles[$indt]}' , '${i}' notitle with errorbars"
			fi
			pt=${pt}${t}
			p=1
			let indt++
		done
		echo "${pt}"
}  | gnuplot -persist
