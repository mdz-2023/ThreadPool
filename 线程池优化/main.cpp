#include <iostream>
#include <thread>
#include <chrono>
#include "threadpool.h"
#include <future>
#include <functional>

using ulong = unsigned long long;

/*
	���̳߳��ύ���������
	1������pool.submitTasks(sum, 10, 20) 
		�û�ֻ��Ҫдһ��������ֱ�ӽ�������Ϊ�̺߳��������Ҵ���ɱ����
		��ôsubmitTasks��Ҫ���ɱ��ģ����

	2���Լ����Result�Լ���ص����ͣ�����̫��
		C++ �߳̿�� thread����û��ʵ�ֽ����̺߳����ķ���ֵ
		�������ṩ�� package_task ����һ��function��������
					async ���ܸ�ǿ��
*/

#if  0
int sum1(int a, int b) {
	return a + b;
}
int main() {
	// ʹ�ú������󣬴��һ������
	std::packaged_task<int(int, int)> task(sum1);

	// ���մ�������У��̺߳����ķ���ֵ������Result��
	std::future<int> res = task.get_future();
	task(10, 20);
	//// �����߳���ִ������
	//std::thread t(std::move(task), 10, 20); // ʹ����ֵ���ÿ�������
	//t.detach();

	std::cout << res.get(); // get�����ȴ������ʱ���������ǰ����


	return 0;
}
#endif

#if 1
int sum(int v1_, int v2_) {
	//std::cout << "my printTask:" << std::this_thread::get_id() << "begin!" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));
	int sum = 0;
	while (v1_ <= v2_) {
		sum += v1_;
		++v1_;
	}
	//std::cout << "my printTask:" << std::this_thread::get_id() << "end!" << std::endl;
	return sum;
}
int main() {
	{
		ThreadPool pool;
		//pool.setMode(PoolMode::MODE_CACHED);
		pool.start(2);

		std::future<int> res1 = pool.submitTasks(sum, 1, 10);
		std::future<int> res2 = pool.submitTasks(sum, 2, 10);
		std::future<int> res3 = pool.submitTasks(sum, 3, 10);
		std::future<int> res4 = pool.submitTasks(sum, 4, 10);
		std::future<int> res5 = pool.submitTasks(sum, 5, 10);

		std::cout << res1.get() << std::endl;
		std::cout << res2.get() << std::endl;
		std::cout << res3.get() << std::endl;
		std::cout << res4.get() << std::endl;
		std::cout << res5.get() << std::endl;
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
#endif