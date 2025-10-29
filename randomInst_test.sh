#!/bin/bash

# 循环一千遍执行pin命令
for i in $(seq 1 1000)
do
    echo "Executing iteration $i"
    pin -t /home/tongshiyu/pin/source/tools/pinfi/obj-intel64/randomInst.so -randinst 1 -- /home/tongshiyu/programs/hpl-2.3/testing/xhpl
done
