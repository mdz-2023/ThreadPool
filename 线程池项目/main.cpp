#include <iostream>
#include <thread>
#include <chrono>
#include "threadpool.h"

using ulong = unsigned long long;

class sumTask : public Task {
public:
	// ����0��run������λ�ȡ������
	// ��printTask�������������͵ĳ�Ա������run��������ֱ�ӷ��ʵ���������
	sumTask(int v1, int v2) 
		:v1_(v1)
		, v2_(v2) 
	{}


	// ����1��run�ķ���ֵ��ô��Ʋ��ܱ�ʾ�������ͣ�
	// C++17�Ѿ�ʵ����Any���ͣ���Ϊ�ϵ��࣬�൱��������Ļ���  
	Any run() { // run�����������̳߳ط�����߳���ִ��
		std::cout << "my printTask:" << std::this_thread::get_id() << "begin!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
		ulong sum = 0;
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
	// ��������򣬲�����������
	ThreadPool pool;
	pool.setMode(PoolMode::MODE_CACHED); // ����Ϊ��̬�仯���̳߳�
	pool.start(4);
	Result res1 = pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	pool.submitTasks(std::make_shared<sumTask>(1, 10000000));
	ulong s1 = res1.get().cast_<ulong>();
	std::cout << s1 << std::endl;
	}
	std::cout << "main over" << std::endl;
#if 0
	{
	// ��������򣬲�����������
	ThreadPool pool;
	// �û��Լ������̳߳ع���ģʽ
	pool.setMode(PoolMode::MODE_CACHED); // ����Ϊ��̬�仯���̳߳�
	pool.start();

	// ����2��������Result���ƣ�
	//Result res = pool.submitTasks(std::make_shared<printTask>());
	// ����ȡ�����ʱ�����û�м��������Ҫ����
	//res.get().cast_<int>(); //res.get()������һ��Any���ͣ�cast_��Ϊģ�庯�����û�������Ҫ������


	// Master-Slever �߳�ģ��
	// Master�߳������ֽ�����Ȼ�����Slave�̷߳�������
	// �ȴ�����Slaver�߳�ִ��������񣬷��ؽ��
	// Master�̺߳ϲ����������������
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
	//std::this_thread::sleep_for(std::chrono::seconds(5)); // ˯��5��
}
#endif
	getchar();
}