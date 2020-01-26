# my_sylar
Pure copy from sylar

## copy rule
1. myself
2. refer sylar

## 栈
https://github.com/Tencent/libco/issues/111

## 分析fiber的use_count
schedule分支 f3f9be7004bde021b5734d24b42107380813d412 在此基础上稍微把idle_fiber改动一下即可 或者直接回退到想要的commit

## 分析 Mainfunc中最后一行swapout后 本fiber的生命周期
fiber分支 071f78a648ff120a88279f26c0b066ee01a43856 稍微改一下即可
