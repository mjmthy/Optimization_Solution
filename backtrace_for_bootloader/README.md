# 需求背景

<div align="center"><img src="images/uboot_crash_scene.png"></div>

<p align="center">P1. bootloader死机现场</p>

如图P1所示，bootloader下如果发生死机，可以获取的所有辅助信息仅仅是一些寄存器的内容。上面的ELR是发生死机时的PC值，我们需要通过addr2line工具获取死机时正在执行的代码，然后进一步分析。很明显得，我们可以感觉到这个分析过程非常不直观。