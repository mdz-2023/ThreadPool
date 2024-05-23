#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include "any.h"
#include "semaphore.h"

// Task类型的前置声明
class Task;

//实现接收提交到线程池的task任务执行完成后的返回值类型Result
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	// 问题1：setValue方法，获取任务执行完的返回值
	void setValue(Any any);
	 
	// 问题2：get方法，用户调用这个方法获取task的返回值
	Any get();
private:
	Any any_; // 存储任务的返回值
	Semaphore sem_; // 线程通信信号量
	std::shared_ptr<Task> task_; // 指向对应获取返回值的任务对象
	std::atomic_bool isValid_; // 返回值是否有效，在submitTask失败的时候就无效
};

// 任务抽象基类
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	// 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;
private:
	Result* result_; // Result生命周期长于task，不用强智能指针，普通指针即可
};

// 线程池支持的模式
enum class PoolMode // 加上class支持命名空间::
{
	MODE_FIXD, // 固定数量的线程
	MODE_CACHED, // 线程数量可动态增长
};

// 线程类型
class Thread
{
public:
	// 线程函数对象类型
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
	~Thread();
	// 开始执行线程函数
	void start();

public:
	unsigned int threadId_; // 本线程id

private:
	ThreadFunc func_;

	static int generate_; // 给所有线程编号
};
/*
example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task {
public:
	void run() { // 线程代码...	}
};

pool.submitTask(std::make_shared<MyTask>());
*/
// 线程池类
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	// 设置线程池的工作模式
	void setMode(PoolMode mode);

	// 设置任务队列上限阈值
	void setTaskQueMaxTreshHold(int threshHold);

	// 设置线程数量上限阈值
	void setThreadMaxTreshHold(int threshHold);

	// 提交任务
	Result submitTasks(std::shared_ptr<Task> sp);

	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency());

	// 禁止用户对线程池对象进行拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc(int threadId);

	// 判断是否运行中
	const bool checkRunningState();
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<unsigned int, std::unique_ptr<Thread>> threads_; // 线程列表
	std::atomic_uint idleThreadSize_; // 空闲线程的数量
	std::atomic_uint threadSize_; // 当前线程池中的所有线程数量
	size_t initThreadSize_; // 初始线程数量
	size_t threadSizeThreshHold_; // 最大线程数量阈值

	PoolMode poolMode_; // 线程池工作模式

	// 防止用户传入的task是临时变量，生命周期过短，使用强智能指针将生命周期拉长
	std::queue<std::shared_ptr<Task>> taskQue_;
	std::atomic_uint taskSize_; // 任务的数量
	int taskQueMaxTreshHold_; // 任务队列数量上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满 
	std::condition_variable notEmpty_; // 表示任务队列不空

	// 表示当前线程池的启动状态，由于多个线程中都有可能运行同一个线程池，所以要线程安全
	std::atomic_bool isPoolRunning_;

	std::condition_variable exitCond_;
};

#endif // !THREADPOOL_H

