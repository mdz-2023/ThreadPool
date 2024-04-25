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
const int THREAD_MAX_IDLE_TIME = 5; // ��λ����

// �̳߳�֧�ֵ�ģʽ
enum class PoolMode // ����class֧�������ռ�::
{
	MODE_FIXD, // �̶��������߳�
	MODE_CACHED, // �߳������ɶ�̬����
};

// �߳�����
class Thread
{
public:
	// �̺߳�����������
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generate_++)
	{ }

	~Thread() {	}

	// ��ʼִ���̺߳���
	void start()
	{
		// ����һ���߳���ִ�к�����ͬʱ�������
		std::thread t(func_, threadId_);
		// ���÷����̣߳������̶߳�����߳�������ִ�й��̣�
		// ��ֹt�������������������¶��߳�
		t.detach();
	}

public:
	unsigned int threadId_; // ���߳�id

private:
	ThreadFunc func_;

	static int generate_; // �������̱߳��
};
int Thread::generate_ = 0;


// �̳߳���
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
		// ���̳߳���ص��߳���Դȫ������
		// ʹ��������ִ�е��߳��´�while(isPoolRunning_) ѭ��ʱ����ѭ��
		isPoolRunning_ = false;

		//// ʹ���еȴ�״̬���߳� ��Ϊ ����״̬
		//notEmpty_.notify_all();

		// �ȴ��̳߳��������̷߳��أ��߳�ͬ��
		// ������״̬������ & ����ִ��������
		std::unique_lock<std::mutex> lock(taskQueMtx_);

		// �ڴ˴�����notify��������3�У����߳��Ȼ�ȡ��������
		notEmpty_.notify_all();

		exitCond_.wait(lock, [&]()->bool {return threadSize_ == 0; }); // ע��˴����ܻ��������̣߳���Ҫ��func��notifyһ��
	}

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRunningState()) {
			return;
		}
		poolMode_ = mode;
	}

	// �����������������ֵ
	void setTaskQueMaxTreshHold(int threshHold)
	{
		if (checkRunningState()) {
			return;
		}
		taskQueMaxTreshHold_ = threshHold;
	}

	// �����߳�����������ֵ
	void setThreadMaxTreshHold(int threshHold)
	{
		if (checkRunningState() && poolMode_ != PoolMode::MODE_CACHED) {
			return;
		}
		threadSizeThreshHold_ = threshHold;
	}

	// �ύ����
	// ʹ�ÿɱ��ģ���̣����Խ��� ���������� �� ���������Ĳ���
	// ��pool.submitTasks(sum, 10, 20)
	template <typename Func, typename... Args>
	auto submitTasks(Func&& func, Args&&... args)->std::future<decltype(func(args...))> 
		// ����ֵ��future��������ʹ��decltype�����Ƶ�
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>( // RType()�����εĺ���������Ϊ��������һ�б��󶨵�����func��
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)); // ʹ��forward�� ����ת�� ������ ��ֵ����ֵ ����
		std::future<RType> result = task->get_future();

		// ��ȡ��
		std::unique_lock<std::mutex> ulock(taskQueMtx_); // �����������������

		// �̵߳�ͨ��  �ȴ���������п���,�������������1s�������ж��ύ����ʧ�ܣ�����
		// lambda���ʽд����wait_for���ȴ�1���ӣ�1s�ڻ�ȡ�������򷵻�true�����򷵻�false
		if (!notFull_.wait_for(ulock, std::chrono::seconds(1),
			[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxTreshHold_; })) {
			// �ȴ�1s����δ����
			std::cerr << "task queue is full, submit task fail." << std::endl;
			// ����һ����ֵ�����������û��߳�
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		}

		// ������������ɷ���
		//taskQue_.emplace(task);
		// using Task = std::function<void()>;
		// ��ŵ����޲��޷���ֵ�ĺ������󣬿���ͨ���м����� �в��з���ֵ תΪ�޲��޷���
		// ָ������õõ�packaged_task<RType()>����()ʹ�����У������շ���ֵ���˴�(��)�޲�����Ϊ�����Ѿ��󶨵�����������
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		// ֪ͨ
		notEmpty_.notify_all();// ʹ���������������ã�֪ͨ�����߿���������

		// cacheģʽ��������ȽϽ�����������С���������
		// �������֮���ж��Ƿ�Ҫ�����߳�����
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& threadSize_ < threadSizeThreshHold_) {
			std::cout << ">>> create new thread" << std::endl;
			// �������߳�
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			auto threadId = ptr->threadId_;
			threads_.insert({ ptr->threadId_, std::move(ptr) }); // move֮��ԭptr�Ѿ�ʧЧ
			threads_[threadId]->start(); // �����߳�
			threadSize_++;
			idleThreadSize_++;
		}

		/*
		����ֵResult ˼����
		��������Ҫ��Ե��� Task �� Result
		���������ַ�ʽ��
		1��return task->getResult();
		2��return Result(task);
			�𣺷�ʽ1�����У���Ϊtask���������ޣ�
			���̴߳Ӷ�����ȡ������ִ�����֮��task�ͱ�������
			���û���ȡ�𰸵�ʱ���϶�����task������֮�󣬿϶���ȡ����task�ĳ�ԱResult��
			ʹ�÷���2��ͨ������ָ�뽫task�������ӳ���Result�����У�
			���û�ʹ����Result����������������task����

		������ʱ����Result�еĳ�Ա������ֹ����ֵ���ÿ����͸�ֵ��
		���������Զ�ƥ����ֵ���ÿ����͸�ֵ
		*/
		return result;
	}

	// �����̳߳�
	void start(int initThreadSize)
	{
		isPoolRunning_ = true;
		initThreadSize_ = initThreadSize;
		threadSize_ = initThreadSize_;

		// �����߳� std::vector<Thread*> threads_;
		for (int i = 0; (size_t)i < initThreadSize_; ++i) {
			// �����̣߳�ʹ�ð����������߳�Ҫִ�еĺ���
			// ��new��Ҫ��delete����ͨnew��������Ҫ�ں��ʵ�ʱ���ֶ�delete
			// make_unique��C++14�ı�׼��
			// ������ָ����Ա���������⣬
			// uniqueptr���ε�ʱ���ֹ����ֵ��������͸�������
			// ��������ֵ���õĿ�����ֵ����
			// ��Ҫͨ��move���������Դת��
			// ����threadFunc��Ҫһ������������ʹ�ò���ռλ��placeholders
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));
			threads_.insert({ ptr->threadId_, std::move(ptr) });
		}

		// �����̣߳�ִ�к���
		for (auto&& [_, value] : threads_) {
			value->start();
			idleThreadSize_++;
		}
	}

	// ��ֹ�û����̳߳ض�����п�������͸�ֵ
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����̺߳���
	void threadFunc(int threadId)
	{
		// �ڱ������м�¼���̵߳Ĵ��ʱ�䣬�Գ����߳�������ֵ���߳̽��л���
		auto lastTime = std::chrono::high_resolution_clock().now();

		// �̺߳�����Ҫһ��ȥ��������л�ȡ����ִ�У����̳߳�������һֱ��ѭ��
		// ��Դ���գ���������ִ��������ͷ��߳�
		while (1) {
			Task task;
			{
				// ��ȡ��
				std::unique_lock<std::mutex> ulock(taskQueMtx_);
				std::cout << "tid:" << std::this_thread::get_id() << "���Ի�ȡ����..." << std::endl;

				// �� + ˫���жϣ���ֹ����ʱ���3�У����������Ȼ�ȡ��������
				while (taskQue_.size() == 0) { // ��ʱ��û�������ʱ�򣬲Ż���������߳�
					// �������Ϊ��ʱ�ſ��Ի����߳�
					// ͨ���� + ˫���жϣ�������ģʽ�µ��߳��ͷŲ����ϲ���һ��
					if (!isPoolRunning_) {
						threads_.erase(threadId);
						threadSize_--;
						exitCond_.notify_all();
						std::cout << ">>> threadId:" << std::this_thread::get_id() << "  exit" << std::endl;
						return; // �����̺߳������ǽ�����ǰ�߳�
					}
					// cacheģʽ�£�������ֵ�������̣߳�������ʱ�䳬��60s��Ӧ������
					if (poolMode_ == PoolMode::MODE_CACHED) {
						// ÿһ���ӷ���һ�Σ�ע������ ��ʱ���� �� ���������ִ�з���
						// ����������ʱ�����ˣ������߳���һ������û���䵽����
						if (std::cv_status::timeout == notEmpty_.wait_for(ulock, std::chrono::seconds(1))) {
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() > THREAD_MAX_IDLE_TIME && threadSize_ > initThreadSize_) {
								// ��ʼ�����߳�
								// 1����¼�߳������ı�����ֵ�޸�
								// 2�����̶߳����vector��ɾ����
								// ������map������̶߳��󣬲���ÿ���߳���һ����Ա����threadId
								threads_.erase(threadId);
								threadSize_--;
								idleThreadSize_--;
								std::cout << ">>> threadId:" << std::this_thread::get_id() << "  exit" << std::endl;
								return;
							}
						}
						// ���������������������أ�ͬ���д����߼�
					}
					else { // fixedģʽ
						// �ȴ�notEmpty�������ȵ�֮�������taskQueMtx_���������󻹻��ж�taskQue_.size() == 0����Ϊ0������ִ��
						notEmpty_.wait(ulock);
					}
					// �˴�������wait�������ص��߼����ж��̳߳��Ƿ����У��ͷű��߳���Դ
					//if (!isPoolRunning_) {
					//	threads_.erase(threadId);
					//	threadSize_--;
					//	exitCond_.notify_all();
					//	std::cout << ">>> threadId:" << std::this_thread::get_id() << "  exit" << std::endl;
					//	return; // �����̺߳������ǽ�����ǰ�߳�
					//}
				}
				idleThreadSize_--;

				std::cout << "tid:" << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;

				// �ǿ�֮�󣬽������������
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				// �����Ȼ��ʣ�����񣬼���֪ͨ�����߳�ȡ����
				if (!taskQue_.empty()) {
					notEmpty_.notify_all();
				}

				// notFull��������
				notFull_.notify_all();
			}//���������Զ��ͷ�ulock��

			// ���߳�ִ���������
			if (task != nullptr) {
				// 1��ִ������
				// 2�����񷵻�ֵsetValue��������Result
				task();
				//task->run(); // ����ָ��ָ��������ķ���
			}
			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
		}
	}

	// �ж��Ƿ�������
	const bool checkRunningState()
	{
		return isPoolRunning_;
	}
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	std::unordered_map<unsigned int, std::unique_ptr<Thread>> threads_; // �߳��б�
	std::atomic_uint idleThreadSize_; // �����̵߳�����
	std::atomic_uint threadSize_; // ��ǰ�̳߳��е������߳�����
	size_t initThreadSize_; // ��ʼ�߳�����
	size_t threadSizeThreshHold_; // ����߳�������ֵ

	PoolMode poolMode_; // �̳߳ع���ģʽ

	// ��ֹ�û������task����ʱ�������������ڹ��̣�ʹ��ǿ����ָ�뽫������������
	using Task = std::function<void()>; // �������󣬷���ֵ��void������(����)����ΪҪ��Ϊ�м��
	std::queue <Task> taskQue_;
	std::atomic_uint taskSize_; // ���������
	int taskQueMaxTreshHold_; // �����������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в��� 
	std::condition_variable notEmpty_; // ��ʾ������в���

	// ��ʾ��ǰ�̳߳ص�����״̬�����ڶ���߳��ж��п�������ͬһ���̳߳أ�����Ҫ�̰߳�ȫ
	std::atomic_bool isPoolRunning_;

	std::condition_variable exitCond_;
};

#endif // !THREADPOOL_H


// �û����øýӿڣ����뺯��������������
// ���������������߳��������ж��Ƿ���Ҫ�����µ��߳�

