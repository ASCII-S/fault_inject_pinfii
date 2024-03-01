#!/bin/bash
# execute this .sh after all_in_one.sh 1 and 2

csv_folder='Result_csv'

if [ $# -lt 1 ]; then 
	echo 没有命令行参数,输入要在当前文件夹下创建的备份文件夹名,请输入日期
	exit 1
fi

csv_folder=$csv_folder$1

if [ ! -d "./$csv_folder" ];then
	mkdir ./$csv_folder
	echo 'makdir '"./$csv_folder"
else
	echo "已经有这个文件夹了"
	#exit 1	
fi
array=( amg  backprop  bfs  data  FFT  hpl  knn  lu )
#array=( lu miniFE )
filearray=(Format_Result.csv)
myfolder=/home/sunzhongao/
 
for item in "${array[@]}"
do
	for file in "${filearray[@]}"
	do
		name=${item}
	        savename=${name}'.csv'
        	desfolder=./$csv_folder/$savename
		if test $item = "miniFE" || test $item = "HPCCG"
		then
			cp ../$item/$item/$file $desfolder
		else
 			cp ../$item/$file $desfolder
		fi
		echo $file in $name has copy to $desfolder
	done
done

python to_excel.py $csv_folder
mv ./result.xls $csv_folder
cp -r $csv_folder $myfolder
echo $csv_folder'has been copied to '$myfolder
