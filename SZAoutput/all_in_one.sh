#!/bin/sh
# 1 args followed, args '1' represents using output_classification.py and '2' stands for csv_process.py
# array is a queue in which you generate csv and xls

array=( amg FFT miniFE backprop Kmeans lu needle HPCCG hpl bfs knn)
#array=( lu )
#fdname='/root/localTool/pin/source/tools/pinfii/example/SZAoutput'

fdname=$(pwd)
echo $fdname

if test $# = 0;then
	echo "maybe you should choose from 1 and 2"
	echo "1 : do output_classification.py for all ../benchmarks"
	echo "2 : do csv_processor.py for all ../benchmarks"
	exit 1
fi


for item in "${array[@]}"
do	
	name=${item}
	cd $fdname
	cd ..
	
	if [ ! -d "$name" ]; then
		echo -e 'No folder: '$item "\n"
		cd $fdname
		continue
	fi

	cd $name
	
	if test $name = 'HPCCG' || test $name = 'miniFE'
	then
                cd $name
	fi 
	
	if test $1 = '1' ;then
		echo "$name ouput_classification.py start......"
		python $fdname/output_classification.py ./ $name 
		echo -e "$item output_classification.py done!\n" 
	elif test $1 = '2' ;then
		echo "$name csv_processor.py start......"
		python $fdname/csv_processor.py ./ $name
                echo -e "$item csv_processor.py done!\n"
	fi
	
done


echo "${array[@]}"' have been done by option '"$1"
