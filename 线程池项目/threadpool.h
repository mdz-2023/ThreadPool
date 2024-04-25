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

// Task���͵�ǰ������
class Task;

//ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result {
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	// ����1��setValue��������ȡ����ִ����ķ���ֵ
	void setValue(Any any);
	 
	// ����2��get�������û��������������ȡtask�ķ���ֵ
	Any get();
private:
	Any any_; // �洢����ķ���ֵ
	Semaphore sem_; // �߳�ͨ���ź���
	std::shared_ptr<Task> task_; // ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_; // ����ֵ�Ƿ���Ч����submitTaskʧ�ܵ�ʱ�����Ч
};

// ����������
class Task
{
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
private:
	Result* result_; // Result�������ڳ���task������ǿ����ָ�룬��ָͨ�뼴��
};

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
	Thread(ThreadFunc func);
	~Thread();
	// ��ʼִ���̺߳���
	void start();

public:
	unsigned int threadId_; // ���߳�id

private:
	ThreadFunc func_;

	static int generate_; // �������̱߳��
};
/*
example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task {
public:
	void run() { // �̴߳���...	}
};

pool.submitTask(std::make_shared<MyTask>());
*/
// �̳߳���
class ThreadPool
{
public:
	ThreadPool();
	~ThreadPool();

	// �����̳߳صĹ���ģʽ
	void setMode(PoolMode mode);

	// �����������������ֵ
	void setTaskQueMaxTreshHold(int threshHold);

	// �����߳�����������ֵ
	void setThreadMaxTreshHold(int threshHold);

	// �ύ����
	Result submitTasks(std::shared_ptr<Task> sp);

	// �����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency());

	// ��ֹ�û����̳߳ض�����п�������͸�ֵ
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	// �����̺߳���
	void threadFunc(int threadId);

	// �ж��Ƿ�������
	const bool checkRunningState();
private:
	//std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	std::unordered_map<unsigned int, std::unique_ptr<Thread>> threads_; // �߳��б�
	std::atomic_uint idleThreadSize_; // �����̵߳�����
	std::atomic_uint threadSize_; // ��ǰ�̳߳��е������߳�����
	size_t initThreadSize_; // ��ʼ�߳�����
	size_t threadSizeThreshHold_; // ����߳�������ֵ

	PoolMode poolMode_; // �̳߳ع���ģʽ

	// ��ֹ�û������task����ʱ�������������ڹ��̣�ʹ��ǿ����ָ�뽫������������
	std::queue<std::shared_ptr<Task>> taskQue_;
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

