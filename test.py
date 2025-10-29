
stacksize = "spsize"
def get_stack_size():
    size = ""
    with open(stacksize,"r") as f:  ##stacksize中保存的是ins所在函数的第一个包含sub和rsp的汇编指令,即为局部变量分配空间的指令,由于汇编代码中会集中把局部变量放在开头分配,因此size为该函数为局部变量预留的空间
        intsize = 0
        lines = f.readlines()
        for line in lines:
            line = line.rstrip("\n")
            if "push" in line:
                intsize += 8
            if "sub" in line:
                items = line.split(",")
                subsize = items[len(items)-1]
                intsize += int(subsize,16)
    size = str(hex(intsize))
    return size

print(get_stack_size())

def get_stack_size2():
    size = ""
    with open(stacksize,"r") as f:  ##stacksize中保存的是ins所在函数的第一个包含sub和rsp的汇编指令,即为局部变量分配空间的指令,由于汇编代码中会集中把局部变量放在开头分配,因此size为该函数为局部变量预留的空间
        
        lines = f.readlines()
        for line in lines:
            line = line.rstrip("\n")
            if "sub" in line:
                items = line.split(",")
                size = items[len(items)-1]
    return size

print(get_stack_size2())