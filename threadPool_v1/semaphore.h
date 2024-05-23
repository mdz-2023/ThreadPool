#pragma once
#ifndef SEMAPHORE_H
#include <mutex>
#include <condition_variable>
class Semaphore
{
public:
	// resLimit初始设置为0，等待线程将结果准备好再使信号量增加
	Semaphore(int resLimit = 0) // 重大BUG，本来写成1了！！！！
		: resLimit_(resLimit) 
	{ }
	~Semaphore() = default;

	// 获取一个信号量资源 P操作
	void wait() {
		std::unique_lock<std::mutex> ulock(mtx_);
		cond_.wait(ulock, [&]()->bool { return resLimit_ > 0; });
		resLimit_--;
	}

	// 增加一个信号量资源 V操作
	void post() {
		std::unique_lock<std::mutex> ulock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};
#define SEMAPHORE_H
#endif // !SEMAPHORE_H


