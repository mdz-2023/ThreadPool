#include <iostream>
#include <thread>
#include <chrono>
#include "threadpool.h"
#include <future>
#include <functional>

using ulong = unsigned long long;

/*
	让线程池提交任务更方便
	1、形如pool.submitTasks(sum, 10, 20) 
		用户只需要写一个函数，直接将函数作为线程函数，并且传入可变参数
		那么submitTasks需要：可变参模板编程

	2、自己造的Result以及相关的类型，代码太多
		C++ 线程库的 thread对象，没有实现接收线程函数的返回值
		所以其提供了 package_task （是一个function函数对象）
					async 功能更强大
*/

#if  0
int sum1(int a, int b) {
	return a + b;
}
int main() {
	// 使用函数对象，打包一个任务
	std::packaged_task<int(int, int)> task(sum1);

	// 接收打包任务中，线程函数的返回值。类似Result类
	std::future<int> res = task.get_future();
	task(10, 20);
	//// 在子线程中执行任务
	//std::thread t(std::move(task), 10, 20); // 使用右值引用拷贝构造
	//t.detach();

	std::cout << res.get(); // get函数等待结果的时候会阻塞当前进程


	return 0;
}
#endif

#if 1
int sum(int v1_, int v2_) {
	//std::cout << "my printTask:" << std::this_thread::get_id() << "begin!" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));
	int sum = 0;
	while (v1_ <= v2_) {
		sum += v1_;
		++v1_;
	}
	//std::cout << "my printTask:" << std::this_thread::get_id() << "end!" << std::endl;
	return sum;
}
int main() {
	{
		ThreadPool pool;
		//pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);

		std::future<int> res1 = pool.submitTasks(sum, 1, 10);
		std::future<int> res2 = pool.submitTasks(sum, 2, 10);
		std::future<int> res3 = pool.submitTasks(sum, 3, 10);
		std::future<int> res4 = pool.submitTasks(sum, 4, 10);
		std::future<int> res5 = pool.submitTasks(sum, 5, 10);

		std::cout << res1.get() << std::endl;
		std::cout << res2.get() << std::endl;
		std::cout << res3.get() << std::endl;
		std::cout << res4.get() << std::endl;
		std::cout << res5.get() << std::endl;
	}
	std::cout << "main over" << std::endl;
#if 0
	{
	// 添加作用域，测试析构函数
	ThreadPool pool;
	// 用户自己设置线程池工作模式
	pool.setMode(PoolMode::MODE_CACHED); // 设置为动态变化的线程池
	pool.start();

	// 问题2：如何设计Result机制？
	//Result res = pool.submitTasks(std::make_shared<printTask>());
	// 但获取结果的时候，如果没有计算完成需要阻塞
	//res.get().cast_<int>(); //res.get()将返回一个Any类型，cast_作为模板函数由用户传入想要的类型


	// Master-Slever 线程模型
	// Master线程用来分解任务，然后各个Slave线程分配任务
	// 等待各个Slaver线程执行完成任务，返回结果
	// Master线程合并各个任务结果，输出
	Result res1 = pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	Result res2 = pool.submitTasks(std::make_shared<sumTask>(10000001, 20000000));
	Result res3 = pool.submitTasks(std::make_shared<sumTask>(20000001, 30000000));
	Result res4 = pool.submitTasks(std::make_shared<sumTask>(1, 10000000));

	Result res5 = pool.submitTasks(std::make_shared<sumTask>(10000001, 20000000));
	Result res6 = pool.submitTasks(std::make_shared<sumTask>(20000001, 30000000));

	ulong s1 = res1.get().cast_<ulong>();
	ulong s2 = res2.get().cast_<ulong>();
	ulong s3 = res3.get().cast_<ulong>();
	std::cout << s1 + s2 + s3 << std::endl;

	//ulong sum = 0;
	//for (auto i = 1; i < 30000001; ++i) {
	//	sum += i;
	//}
	//std::cout << sum;
	//std::this_thread::sleep_for(std::chrono::seconds(5)); // 睡眠5秒
}
#endif
	getchar();
}
#endif