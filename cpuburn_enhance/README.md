# 什么是cpuburn

cpuburn是一个可以让所有CPU核的使用率达到100%的压力测试工具。

![cpuburn_overview](images/cpuburn_overview.png)
<center>P1 cpuburn_overview</center>

如上图所示，cpuburn会在每个核上运行一个线程，该线程以死循环的方式持续进行NEON寄存器的数学运算，来使CPU的负载达到100%。

# 当前CPUBURN存在的不足

原始的cpuburn只能以always on的方式运行，无法支持负载高低的快速切换。之所以有这样的需求，是因为