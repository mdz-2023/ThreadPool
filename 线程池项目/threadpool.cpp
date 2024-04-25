#include "threadpool.h"
#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 5; // ��λ����

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
/// ����ʱ�������߳����������
/// 1�����ڵȴ������� ��wait
/// 2������ִ�����������
/// 3������ִ�н������ոս���while(isPoolRunning_)ѭ���� 
///		��ʱ��exitCond_��wait����threadSize_ == 0������ͷ���������ȴ�״̬��
///		threadFunc�����õ���Ȼ��notEmpty_.wait(ulock)�ȴ�notEmprty������Ҳ���ͷ���������ȴ�״̬;
///		����ʱ��������exitCond_��notEmpty_��notify ���� �������ط���ʹthreadSize_ == 0������
///		��������������̵߳�������
/// </summary>
ThreadPool::~ThreadPool()
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

// �û����øýӿڣ��������������������
// ���������������߳��������ж��Ƿ���Ҫ�����µ��߳�
Result ThreadPool::submitTasks(std::shared_ptr<Task> sp)
{	
	// ��ȡ��
	std::unique_lock<std::mutex> ulock(taskQueMtx_); // �����������������

	// �̵߳�ͨ��  �ȴ���������п���
	// �û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����

	/*
	//while (taskSize_ >= taskQueMaxTreshHold_) {
	//	std::cout << "submit fail";
	//	notFull_.wait(ulock); // �ȴ� �������������� ����ʹ�ã����ȴ��������߳�����
	//}// ��wait�õ���������ʱ��ulock��������
	*/

	/*
	//lambda���ʽд����һֱ�ȴ�������true
	//notFull_.wait(ulock, [&]()->bool { 
	//	return taskQue_.size() < taskQueMaxTreshHold_;
	//	});
	*/

	/*
		wait  һֱ�ȵ���
		wait_for  �����ȴ�һ��ʱ�䣬false����ʾ�ȴ���ʱ�������δ����
		wait_until  �����ȵ�ĳ��ʱ�䣨����ȵ�����һ��
	*/
	
	//lambda���ʽд����wait_for���ȴ�1���ӣ�1s�ڻ�ȡ�������򷵻�true�����򷵻�false
	if (!notFull_.wait_for(ulock, std::chrono::seconds(1), 
		[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxTreshHold_; })) {
		// �ȴ�1s����δ����
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return Result(sp, false);
	}

	// ������������ɷ���
	taskQue_.emplace(sp);
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
	return Result(sp);
}

void ThreadPool::start(int initThreadSize)
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

// �̳߳ص������̣߳��������������������
void ThreadPool::threadFunc(int threadId)
{
	// �ڱ������м�¼���̵߳Ĵ��ʱ�䣬�Գ����߳�������ֵ���߳̽��л���
	auto lastTime = std::chrono::high_resolution_clock().now();

	// �̺߳�����Ҫһ��ȥ��������л�ȡ����ִ�У����̳߳�������һֱ��ѭ��
	// ��Դ���գ���������ִ��������ͷ��߳�
	while(1) {
		std::shared_ptr<Task> task;
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
			task->exec();
			//task->run(); // ����ָ��ָ��������ķ���
		}
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); // �����߳�ִ���������ʱ��
	}
}

const bool ThreadPool::checkRunningState()
{
	return isPoolRunning_;
}

/// <summary>
/// Thread���Ա�����Ķ���
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
	// ����һ���߳���ִ�к�����ͬʱ�������
	std::thread t(func_, threadId_);
	// ���÷����̣߳������̶߳�����߳�������ִ�й��̣�
	// ��ֹt�������������������¶��߳�
	t.detach();
}

/// <summary>
/// Result�����Ա�����Ķ���
/// </summary>
/// <param name="task"></param>
/// <param name="isValid"></param>
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

// ���ô�����task�е�exec�����У�ͨ��resultָ��������
void Result::setValue(Any any)
{
	this->any_ = std::move(any); // ʹ����ֵ���ý�����Դת��
	sem_.post(); // �Ѿ���ȡ����ֵ�����������ź���Դ
}

// �û�����
Any Result::get()
{
	if(!isValid_) return "";
	sem_.wait(); // �ȴ��ź������ã����ȴ�����������û��߳�
	return std::move(any_); // ʹ����ֵ���ã���ֹ��������
}


Task::Task() 
	: result_(nullptr)
{
}

void Task::exec()
{
	if (result_ != nullptr) {
		result_->setValue(run());// ��̬�����ĵط�
	}
	//Any any = this->run();
	//result_->setValue(any);// ��̬�����ĵط�
}

// ��resultͨ�����Ա����task������
void Task::setResult(Result* result)
{
	result_ = result;
}
