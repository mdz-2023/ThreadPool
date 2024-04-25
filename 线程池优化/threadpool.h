#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <future>

const int TASK_MAX_THRESHHOLD = 2;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 5; // 单位：秒

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
	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generate_++)
	{ }

	~Thread() {	}

	// 开始执行线程函数
	void start()
	{
		// 创建一个线程来执行函数，同时传入参数
		std::thread t(func_, threadId_);
		// 设置分离线程，分离线程对象和线程真正的执行过程，
		// 防止t出作用域被析构，产生孤儿线程
		t.detach();
	}

public:
	unsigned int threadId_; // 本线程id

private:
	ThreadFunc func_;

	static int generate_; // 给所有线程编号
};
int Thread::generate_ = 0;


// 线程池类
class ThreadPool
{
public:
	ThreadPool()
		:initThreadSize_(0)
		, taskSize_(0)
		, taskQueMaxTreshHold_(TASK_MAX_THRESHHOLD)
		, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXD)
		, isPoolRunning_(false)
		, idleThreadSize_(0)
	{
	}
	~ThreadPool()
	{
		// 将线程池相关的线程资源全部回收
		// 使所有正在执行的线程下次while(isPoolRunning_) 循环时跳出循环
		isPoolRunning_ = false;

		//// 使所有等待状态的线程 变为 阻塞状态
		//notEmpty_.notify_all();

		// 等待线程池中所有线程返回，线程同步
		// 有两种状态：阻塞 & 正在执行任务中
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		// 在此处进行notify，解决情况3中，子线程先获取锁的问题
		notEmpty_.notify_all();

		exitCond_.wait(lock, [&]()->bool {return threadSize_ == 0; }); // 注意此处可能会阻塞主线程，需要在func中notify一下
	}

	// 设置线程池的工作模式
	void setMode(PoolMode mode)
	{
		if (checkRunningState()) {
			return;
		}
		poolMode_ = mode;
	}

	// 设置任务队列上限阈值
	void setTaskQueMaxTreshHold(int threshHold)
	{
		if (checkRunningState()) {
			return;
		}
		taskQueMaxTreshHold_ = threshHold;
	}

	// 设置线程数量上限阈值
	void setThreadMaxTreshHold(int threshHold)
	{
		if (checkRunningState() && poolMode_ != PoolMode::MODE_CACHED) {
			return;
		}
		threadSizeThreshHold_ = threshHold;
	}

	// 提交任务
	// 使用可变参模板编程，可以接收 任意任务函数 和 任意数量的参数
	// 即pool.submitTasks(sum, 10, 20)
	template <typename Func, typename... Args>
	auto submitTasks(Func&& func, Args&&... args)->std::future<decltype(func(args...))> 
		// 返回值是future，但类型使用decltype进行推导
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>( // RType()不带参的函数对象，因为参数在下一行被绑定到函数func中
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)); // 使用forward来 完美转发 参数的 左值或右值 属性
		std::future<RType> result = task->get_future();

		// 获取锁
		std::unique_lock<std::mutex> ulock(taskQueMtx_); // 构造加锁，析构解锁

		// 线程的通信  等待任务队列有空余,最长不能阻塞超过1s，否则判断提交任务失败，返回
		// lambda表达式写法，wait_for最多等待1秒钟，1s内获取到变量则返回true，否则返回false
		if (!notFull_.wait_for(ulock, std::chrono::seconds(1),
			[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxTreshHold_; })) {
			// 等待1s条件未满足
			std::cerr << "task queue is full, submit task fail." << std::endl;
			// 返回一个空值，不会阻塞用户线程
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		}

		// 如果不满，即可放入
		//taskQue_.emplace(task);
		// using Task = std::function<void()>;
		// 存放的是无参无返回值的函数对象，可以通过中间层进行 有参有返回值 转为无参无返回
		// 指针解引用得到packaged_task<RType()>，加()使其运行，不接收返回值。此处(中)无参是因为参数已经绑定到函数对象中
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		// 通知
		notEmpty_.notify_all();// 使不空条件变量可用，通知消费者可以消费了

		// cache模式：任务处理比较紧急，场景是小而快的任务
		// 添加任务之后，判断是否要增加线程数量
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& threadSize_ < threadSizeThreshHold_) {
			std::cout << ">>> create new thread" << std::endl;
			// 创建新线程
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			auto threadId = ptr->threadId_;
			threads_.insert({ ptr->threadId_, std::move(ptr) }); // move之后，原ptr已经失效
			threads_[threadId]->start(); // 启动线程
			threadSize_++;
			idleThreadSize_++;
		}

		/*
		返回值Result 思考：
		我们现有要面对的是 Task 和 Result
		返回有两种方式：
		1、return task->getResult();
		2、return Result(task);
			答：方式1不可行，因为task作用域有限，
			在线程从队列中取出任务并执行完毕之后，task就被析构，
			而用户获取答案的时机肯定是在task被析构之后，肯定获取不到task的成员Result。
			使用方法2，通过智能指针将task的寿命延长在Result对象中，
			在用户使用完Result对象并析构后再析构task对象

		返回临时对象，Result中的成员变量禁止了左值引用拷贝和赋值，
		编译器会自动匹配右值引用拷贝和赋值
		*/
		return result;
	}

	// 开启线程池
	void start(int initThreadSize)
	{
		isPoolRunning_ = true;
		initThreadSize_ = initThreadSize;
		threadSize_ = initThreadSize_;

		// 创建线程 std::vector<Thread*> threads_;
		for (int i = 0; (size_t)i < initThreadSize_; ++i) {
			// 创建线程，使用绑定器，传入线程要执行的函数
			// 有new就要有delete，普通new出来的需要在合适的时间手动delete
			// make_unique是C++14的标准？
			// 用智能指针可以避免此类问题，
			// uniqueptr传参的时候禁止了左值拷贝构造和复制重载
			// 但是有右值引用的拷贝赋值函数
			// 需要通过move语义进行资源转移
			// 由于threadFunc需要一个参数，所以使用参数占位符placeholders
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));
			threads_.insert({ ptr->threadId_, std::move(ptr) });
		}

		// 开启线程，执行函数
		for (auto&& [_, value] : threads_) {
			value->start();
			idleThreadSize_++;
		}
	}

	// 禁止用户对线程池对象进行拷贝构造和赋值
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// 定义线程函数
	void threadFunc(int threadId)
	{
		// 在本函数中记录此线程的存活时间，对超过线程数量阈值的线程进行回收
		auto lastTime = std::chrono::high_resolution_clock().now();

		// 线程函数需要一种去任务队列中获取任务并执行，在线程池运行中一直死循环
		// 资源回收：所有任务都执行完后再释放线程
		while (1) {
			Task task;
			{
				// 获取锁
				std::unique_lock<std::mutex> ulock(taskQueMtx_);
				std::cout << "tid:" << std::this_thread::get_id() << "尝试获取任务..." << std::endl;

				// 锁 + 双重判断，防止析构时情况3中，析构函数先获取锁的问题
				while (taskQue_.size() == 0) { // 长时间没有任务的时候，才会产生空闲线程
					// 任务队列为空时才可以回收线程
					// 通过锁 + 双重判断，将两种模式下的线程释放操作合并到一处
					if (!isPoolRunning_) {
						threads_.erase(threadId);
						threadSize_--;
						exitCond_.notify_all();
						std::cout << ">>> threadId:" << std::this_thread::get_id() << "  exit" << std::endl;
						return; // 结束线程函数就是结束当前线程
					}
					// cache模式下，多于阈值数量的线程，若空闲时间超过60s，应当回收
					if (poolMode_ == PoolMode::MODE_CACHED) {
						// 每一秒钟返回一次，注意区分 超时返回 和 有子任务待执行返回
						// 条件变量超时返回了，即本线程在一秒钟内没分配到任务
						if (std::cv_status::timeout == notEmpty_.wait_for(ulock, std::chrono::seconds(1))) {
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() > THREAD_MAX_IDLE_TIME && threadSize_ > initThreadSize_) {
								// 开始回收线程
								// 1、记录线程数量的变量的值修改
								// 2、把线程对象从vector中删掉，
								// 所以用map来存放线程对象，并对每个线程有一个成员变量threadId
								threads_.erase(threadId);
								threadSize_--;
								idleThreadSize_--;
								std::cout << ">>> threadId:" << std::this_thread::get_id() << "  exit" << std::endl;
								return;
							}
						}
						// 本处是条件变量正常返回，同下行代码逻辑
					}
					else { // fixed模式
						// 等待notEmpty条件，等到之后进行抢taskQueMtx_锁，抢到后还会判断taskQue_.size() == 0，不为0才向下执行
						notEmpty_.wait(ulock);
					}
					// 此处是两个wait正常返回的逻辑，判断线程池是否运行，释放本线程资源
					//if (!isPoolRunning_) {
					//	threads_.erase(threadId);
					//	threadSize_--;
					//	exitCond_.notify_all();
					//	std::cout << ">>> threadId:" << std::this_thread::get_id() << "  exit" << std::endl;
					//	return; // 结束线程函数就是结束当前线程
					//}
				}
				idleThreadSize_--;

				std::cout << "tid:" << std::this_thread::get_id() << "获取任务成功" << std::endl;

				// 非空之后，进行任务的消费
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				// 如果依然有剩余任务，继续通知其他线程取任务
				if (!taskQue_.empty()) {
					notEmpty_.notify_all();
				}

				// notFull条件可用
				notFull_.notify_all();
			}//出作用域，自动释放ulock锁

			// 本线程执行这个任务
			if (task != nullptr) {
				// 1、执行任务
				// 2、任务返回值setValue方法给到Result
				task();
				//task->run(); // 基类指针指向派生类的方法
			}
			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
		}
	}

	// 判断是否运行中
	const bool checkRunningState()
	{
		return isPoolRunning_;
	}
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
	std::unordered_map<unsigned int, std::unique_ptr<Thread>> threads_; // 线程列表
	std::atomic_uint idleThreadSize_; // 空闲线程的数量
	std::atomic_uint threadSize_; // 当前线程池中的所有线程数量
	size_t initThreadSize_; // 初始线程数量
	size_t threadSizeThreshHold_; // 最大线程数量阈值

	PoolMode poolMode_; // 线程池工作模式

	// 防止用户传入的task是临时变量，生命周期过短，使用强智能指针将生命周期拉长
	using Task = std::function<void()>; // 函数对象，返回值是void、不带(参数)是因为要作为中间层
	std::queue <Task> taskQue_;
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


// 用户调用该接口，传入函数对象，生产任务
// 若任务数量多于线程数量，判断是否需要创建新的线程

