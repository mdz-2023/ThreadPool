#pragma once
#ifndef ANY_H
#define ANY_H

#include<memory>
/*
* ʵ��һ���࣬���Դ���������͵�����
* ����ͨ��һ���ķ�����ȡ������
* �ؼ���1�����Ա����������͵����� -- ģ��template
* �ؼ���2������ָ�������������� -- �̳кͶ�̬
*/
class Any
{
public:
	// ���������вι��캯��֮�󣬱����������������޲ι��캯��
	Any() = default;
	~Any() = default;
	// unique_ptrɾ������ֵ�����͸�ֵ����������ֵ���ú͸�ֵ
	// ����Any��Ҳ�����
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	
	// ʹAny�����������͵�����
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{} //ʹ��T���͵�data����ʼ��һ��������

	// ��Any�����д洢��data������ȡ����
	template <typename T>
	T cast_() {
		// �ӻ���ָ��base_�ҵ�����ָ�����������󣬴�������ȡ��data_
		// ����ָ�� ��� ������ָ�룬ʹ��ָ������ת��������Ҫ����RTTI��Run-Time Type Identification������ʱ����ʶ��
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());// ���������͵�ָ������ð�ȫ��ת��Ϊ�������͵�ָ�������
		
		// ��ģ������T��ԭ��Derive�����Ͳ�ͬ����������ת��ʧ��
		if (pd == nullptr) {
			//throw "type is unmatch!";
			return T();
		}
		return pd->data_;
	}
	
private:
	// ��������
	class Base {
	public:
		virtual ~Base() = default;
	};
	// ���������ͣ�����������͵�����
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data) : data_(data) 
		{}
		T data_; // ʵ�ʴ��������������
	};
private:
	std::unique_ptr<Base> base_; // ��̬������ָ�����ָ����������������
	// unique_ptr ɾ������ֵ�����͸�ֵ����������ֵ���ú͸�ֵ
};


#endif // !ANY_H