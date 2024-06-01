#!/bin/sh

# testssh
runtime=5

if [[ $# -eq 0 ]]; then 
        echo 'please input args: '
	echo 'app_folder	:amg,backprop,bfs,data,FFT,HPCCG,hpl,knn,lu,miniFE'
	echo ' official_or_test(input 1 for official,0 for test)'	
	exit 1
fi

echo "文件名:$1"

if [ "$2" = "1" ];then 
	runtime=15000
fi

echo $runtime

if test "$1" = "HPCCG"  || test "$1" = "miniFE"
then
	cd "$1"
	ln -s ../faultinject.py ./
	if [ ! -d "./$1" ];then
		mkdir "$1"
		echo 'already have dir'$1
	fi
	python faultinject.py $1 $runtime
else
	python faultinject.py $1 $runtime
fi
