#!/bin/bash
array=( amg  backprop  bfs  data  FFT  hpl  knn  lu  )
for item in "${array[@]}"
do
	if [ ! -d ../$item/ ];then
		echo $item: 0
		continue
	else
		desfolder=../$item/prog_output
		echo $item:  $(ls -l $desfolder |wc -l )
	fi
done

ddirapp=(HPCCG miniFE)
for item in "${ddirapp[@]}"
do
        if [ ! -d ../$item/ ];then
                echo $item: 0
                continue
        else
		desfolder=../$item/$item/prog_output
                echo $item:  $(ls -l $desfolder |wc -l )
        fi  
done	
