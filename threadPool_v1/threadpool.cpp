#include "threadpool.h"
#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 5; // 单位：秒

ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxTreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXD)
	, isPoolRunning_(false)
	, idleThreadSize_(0)
{
}

/// <summary>
/// 析构时，所有线程有三种情况
/// 1、处于等待条件下 即wait
/// 2、正在执行任务过程中
/// 3、任务执行结束，刚刚进入while(isPoolRunning_)循环： 
///		此时若exitCond_在wait变量threadSize_ == 0，则会释放锁并进入等待状态，
///		threadFunc中能拿到锁然后notEmpty_.wait(ulock)等待notEmprty成立，也会释放锁并进入等待状态;
///		但此时不会再有exitCond_或notEmpty_来notify 或者 有其他地方能使threadSize_ == 0成立，
///		这样就造成两个线程的死锁。
/// </summary>
ThreadPool::~ThreadPool()
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


void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState()) {
		return;
	}
	poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxTreshHold(int threshHold)
{
	if (checkRunningState()) {
		return;
	}
	taskQueMaxTreshHold_ = threshHold;
}

void ThreadPool::setThreadMaxTreshHold(int threshHold)
{
	if (checkRunningState() && poolMode_ != PoolMode::MODE_CACHED) {
		return;
	}
	threadSizeThreshHold_ = threshHold;
}

// 用户调用该接口，传入任务对象，生产任务
// 若任务数量多于线程数量，判断是否需要创建新的线程
Result ThreadPool::submitTasks(std::shared_ptr<Task> sp)
{	
	// 获取锁
	std::unique_lock<std::mutex> ulock(taskQueMtx_); // 构造加锁，析构解锁

	// 线程的通信  等待任务队列有空余
	// 用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回

	/*
	//while (taskSize_ >= taskQueMaxTreshHold_) {
	//	std::cout << "submit fail";
	//	notFull_.wait(ulock); // 等待 不满的条件变量 可以使用，即等待消费者线程消费
	//}// 从wait得到条件变量时，ulock解锁加锁
	*/

	/*
	//lambda表达式写法，一直等待到返回true
	//notFull_.wait(ulock, [&]()->bool { 
	//	return taskQue_.size() < taskQueMaxTreshHold_;
	//	});
	*/

	/*
		wait  一直等到死
		wait_for  持续等待一段时间，false：表示等待到时间后条件未满足
		wait_until  持续等到某个时间（比如等到下周一）
	*/
	
	//lambda表达式写法，wait_for最多等待1秒钟，1s内获取到变量则返回true，否则返回false
	if (!notFull_.wait_for(ulock, std::chrono::seconds(1), 
		[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxTreshHold_; })) {
		// 等待1s条件未满足
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return Result(sp, false);
	}

	// 如果不满，即可放入
	taskQue_.emplace(sp);
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
	return Result(sp);
}

void ThreadPool::start(int initThreadSize)
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

// 线程池的所有线程，从任务队列里消费任务
void ThreadPool::threadFunc(int threadId)
{
	// 在本函数中记录此线程的存活时间，对超过线程数量阈值的线程进行回收
	auto lastTime = std::chrono::high_resolution_clock().now();

	// 线程函数需要一种去任务队列中获取任务并执行，在线程池运行中一直死循环
	// 资源回收：所有任务都执行完后再释放线程
	while(1) {
		std::shared_ptr<Task> task;
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
			task->exec();
			//task->run(); // 基类指针指向派生类的方法
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // 更新线程执行完任务的时间
	}
}

const bool ThreadPool::checkRunningState()
{
	return isPoolRunning_;
}

/// <summary>
/// Thread类成员方法的定义
/// </summary>
/// <param name="func"></param>
int Thread::generate_ = 0;
Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generate_++)
{
}

Thread::~Thread()
{
}

void Thread::start()
{
	// 创建一个线程来执行函数，同时传入参数
	std::thread t(func_, threadId_);
	// 设置分离线程，分离线程对象和线程真正的执行过程，
	// 防止t出作用域被析构，产生孤儿线程
	t.detach();
}

/// <summary>
/// Result的类成员函数的定义
/// </summary>
/// <param name="task"></param>
/// <param name="isValid"></param>
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

// 调用处：在task中的exec函数中，通过result指针来调用
void Result::setValue(Any any)
{
	this->any_ = std::move(any); // 使用右值引用进行资源转移
	sem_.post(); // 已经获取返回值，可以增加信号资源
}

// 用户调用
Any Result::get()
{
	if(!isValid_) return "";
	sem_.wait(); // 等待信号量可用，即等待结果，阻塞用户线程
	return std::move(any_); // 使用右值引用，防止拷贝构造
}


Task::Task() 
	: result_(nullptr)
{
}

void Task::exec()
{
	if (result_ != nullptr) {
		result_->setValue(run());// 多态发生的地方
	}
	//Any any = this->run();
	//result_->setValue(any);// 多态发生的地方
}

// 由result通过其成员变量task来调用
void Task::setResult(Result* result)
{
	result_ = result;
}
