#pragma once
#ifndef SEMAPHORE_H
#include <mutex>
#include <condition_variable>
class Semaphore
{
public:
	// resLimit��ʼ����Ϊ0���ȴ��߳̽����׼������ʹ�ź�������
	Semaphore(int resLimit = 0) // �ش�BUG������д��1�ˣ�������
		: resLimit_(resLimit) 
	{ }
	~Semaphore() = default;

	// ��ȡһ���ź�����Դ P����
	void wait() {
		std::unique_lock<std::mutex> ulock(mtx_);
		cond_.wait(ulock, [&]()->bool { return resLimit_ > 0; });
		resLimit_--;
	}

	// ����һ���ź�����Դ V����
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


