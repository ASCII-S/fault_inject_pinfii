#!/bin/sh

if [[ $# -eq 0 ]]; then 
        echo 'please input args: '
        echo 'app_folder	:amg,backprop,bfs,data,FFT,HPCCG,hpl,knn,lu,miniFE'
        echo 'official_or_test	:input '1' for official, input '0' stands for test'       
        exit 1
fi

if [[ $# -eq 1 ]]; then
	args=''$1''
	echo 'Test Run'
fi
if [[ $# -eq  2  ]]; then
	args=''$1' '$2''
	echo 'Real Run'
fi


if [ ! -d "$1" ]; then
        mkdir "$1"
else
        echo "已经存在该文件夹,正在覆盖,请检查是否需要保留"
        #exit 1   
fi
 
echo "nohup sh runFile.sh "''$args''" > ./$1/file.log 2>&1 &"

nohup sh runFile.sh $args  > ./$1/file.log 2>&1 &
