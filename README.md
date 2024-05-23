# ThreadPool
基于C++实现了一个线程池。
## 第一版v1
手写了上帝类Any类和信号量类，用于获取提交任务的返回值；
将代码在Linux下编译动态库，并解决跨平台产生的问题：https://blog.csdn.net/qq_20756957/article/details/138161669

## 第二版v2
是优化版，使用C++11的future、package_task等新特性，代码更加精简。
