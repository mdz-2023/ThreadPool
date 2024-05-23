#pragma once
#ifndef ANY_H
#define ANY_H

#include<memory>
/*
* 实现一个类，可以存放任意类型的数据
* 可以通过一定的方法获取到数据
* 关键点1、可以保存任意类型的数据 -- 模版template
* 关键点2、可以指向其他任意类型 -- 继承和多态
*/
class Any
{
public:
	// 当声明了有参构造函数之后，编译器不会再生成无参构造函数
	Any() = default;
	~Any() = default;
	// unique_ptr删除了左值拷贝和赋值，保留了右值引用和赋值
	// 所以Any类也是如此
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	
	// 使Any接收任意类型的数据
	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{} //使用T类型的data，初始化一个派生类

	// 把Any对象中存储的data数据提取出来
	template <typename T>
	T cast_() {
		// 从基类指针base_找到它所指向的派生类对象，从其中提取出data_
		// 基类指针 变成 派生类指针，使用指针类型转换，但是要符合RTTI（Run-Time Type Identification，运行时类型识别）
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());// 将基类类型的指针或引用安全地转换为派生类型的指针和引用
		
		// 若模板类型T与原本Derive的类型不同，将会类型转换失败
		if (pd == nullptr) {
			//throw "type is unmatch!";
			return T();
		}
		return pd->data_;
	}
	
private:
	// 基类类型
	class Base {
	public:
		virtual ~Base() = default;
	};
	// 派生类类型，存放任意类型的数据
	template<typename T>
	class Derive : public Base {
	public:
		Derive(T data) : data_(data) 
		{}
		T data_; // 实际存放任意类型数据
	};
private:
	std::unique_ptr<Base> base_; // 多态，基类指针可以指向任意的派生类对象
	// unique_ptr 删除了左值拷贝和赋值，保留了右值引用和赋值
};


#endif // !ANY_H