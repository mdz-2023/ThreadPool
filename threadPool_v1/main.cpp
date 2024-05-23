#include <iostream>
#include <thread>
#include <chrono>
#include "threadpool.h"

using ull = unsigned long long;

class sumTask : public Task {
public:
	// 问题0：run函数如何获取参数？
	// 答：printTask可以有任意类型的成员变量，run函数可以直接访问当做参数。
	sumTask(int v1, int v2) 
		:v1_(v1)
		, v2_(v2) 
	{}


	// 问题1：run的返回值怎么设计才能表示任意类型？
	// C++17已经实现了Any类型，称为上帝类，相当于所有类的基类  
	Any run() { // run方法最终在线程池分配的线程中执行
		std::cout << "my printTask:" << std::this_thread::get_id() << "begin!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
		ull sum = 0;
		while (v1_ <= v2_) {
			sum += v1_;
			++v1_;
		}
		std::cout << "my printTask:" << std::this_thread::get_id() << "end!" << std::endl;
		return sum;
	}
private:
	int v1_;
	int v2_;
};

int main() {
	{
	// 添加作用域，测试析构函数
	ThreadPool pool;
	pool.setMode(PoolMode::MODE_CACHED); // 设置为动态变化的线程池
	pool.start(4);
	Result res1 = pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	ull s1 = res1.get().cast_<ull>();
	std::cout << s1 << std::endl;
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

	ull s1 = res1.get().cast_<ull>();
	ull s2 = res2.get().cast_<ull>();
	ull s3 = res3.get().cast_<ull>();
	std::cout << s1 + s2 + s3 << std::endl;

	//ull sum = 0;
	//for (auto i = 1; i < 30000001; ++i) {
	//	sum += i;
	//}
	//std::cout << sum;
	//std::this_thread::sleep_for(std::chrono::seconds(5)); // 睡眠5秒
}
#endif
	getchar();
}