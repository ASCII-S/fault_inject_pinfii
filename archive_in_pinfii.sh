#!/bin/sh
# sh this file to save all applications in folder archive$1


if [ $# -lt 1 ]; then 
	echo 没有命令行参数,请输入日期
	exit 1
fi

if [ ! -d "archive_$1" ];then
	mkdir archive_$1
else
	echo "文件夹已经存在,请更换文件夹或删除原文件夹"
	exit 1
fi

fold_name=archive_$1

bm_array=(amg backprop bfs data FFT HPCCG hpl knn lu miniFE)

for item in "${bm_array[@]}"
do
	name="$item"
	#name=""$item"_"$1""
	echo $name
	mv ./$item ./$fold_name/$name
done

cp README ./$fold_name/README
 
