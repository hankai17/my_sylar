# my_sylar
Pure copy from sylar: https://github.com/sylar-yin/sylar. It may refer mordor: https://github.com/mozy/mordor

## copy rule
1. myself && test && learn
2. refer sylar

## 栈
https://github.com/Tencent/libco/issues/111

## 分析fiber的use_count
schedule分支 f3f9be7004bde021b5734d24b42107380813d412 在此基础上稍微把idle_fiber改动一下即可 或者直接回退到想要的commit
1. fd_ctx生命周期
  根本是个fd 里面有event_ctx  其里有三剑客  怎么确保三剑客加上之前都为空 
当发生阻塞时addevent把当前fiber保存到三剑客 yeildtohold跑其他    当事件到来则triggerevent加到队列上(!!!!!!!swap函数 使得三剑客为空) 
es抢到后swapin   上层balabala又读到-1了再次addevent  由于上次trigger时用了swap使得三剑客为空 所以没问题

## 分析 Mainfunc中最后一行swapout后 本fiber的生命周期
fiber分支 071f78a648ff120a88279f26c0b066ee01a43856 稍微改一下即可
