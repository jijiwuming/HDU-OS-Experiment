# 操作系统大作业——文件系统
* 模仿FAT32文件系统，使用FAT表实现
* 实现要求的基本功能
* 支持 读/写/隐藏 控制命令（my_ctrl）
* 支持 my_man 查看命令帮助手册
* 开发采用VS,添加了makefile支持gcc编译
#### 注：由于VS本身安全性检查的关系，使用gcc时请将main函数中gets_s(cmdLine);更换为gets(cmdLine);