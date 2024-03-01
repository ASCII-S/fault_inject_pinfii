#!/bin/bash

# 源文件夹
source_folder="../disassemblycodes"  # 修改为相对路径或绝对路径，根据实际情况

# 获取所有.txt文件
txt_files=("amg.txt" "backprop.txt" "bfs.txt" "FFT.txt" "HPCCG.txt" "hpl.txt" "kmeans.txt" "knn.txt" "lu.txt" "miniFE.txt" "needle.txt")

# 遍历文件并复制到相应文件夹
for file in "${txt_files[@]}"; do
    # 提取文件名前缀（去掉.txt部分）
    folder_name="${file%.txt}"

    # 如果目标文件夹存在，则复制文件
    if [ -d "../$folder_name" ]; then
        cp "$source_folder/$file" "../$folder_name/disassemblycode.txt"
    else
        echo "目标文件夹 '../$folder_name' 不存在，跳过文件 '$file' 的复制"
    fi
done

