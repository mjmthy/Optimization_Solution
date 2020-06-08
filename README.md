# Optimization_Solution

​		在项目实践过程中，有些事情如果按默认的操作流程往往是耗时或者达不到预期的，这个时候就需要改变一下思路，借助一些已有的小工具或者改变一下运行上下文等等小的改动，就可以达到事半功倍的效果。

​		本仓库记录的就是这样的一些实际问题
```
  a. pxp_quick_load
```
​		解决的是cadence的模拟环境PXP/Z1下，验证代码修改需要浪费几个小时在内核代码重新加载上的痛点
```
  b.cpuburn_enhance
```
​		原始的cpuburn只支持always run的模式，并不支持快速自由切换占空比(通过快速的不同占空比的cpuburn，可控触发电压周期性的大幅跌落和上升，以此验证在芯片在理论支持的工作电压范围内，是否可以正常工作)。这里的实现可以支持100us精度下，不同占空比的负载控制

