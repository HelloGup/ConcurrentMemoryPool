#pragma once
#include "Common.h"

//定长内存池

//template <size_t N>//非类型模板参数
//class ObjectPool {
//
//};

template <class T>
class ObjectPool {
public:
	T* New() {
		T* obj = nullptr;

		//当自由链表有空闲内存块时使用自由链表内存块
		if (_freeList) {
			//头删
			void* next = *(void**)_freeList;
			obj = (T*)_freeList;
			_freeList = next;
		}
		//否则使用内存池分配内存
		else {
			size_t objSize = sizeof(T);

			//当剩余内存小于T类型大小时，不够创建一个对象，那么就重新申请内存
			if (_reminSize < objSize) {
				_reminSize = 128 * 1024;
				
				//_memory = (char*)malloc(_reminSize);
				//直接操作堆，不使用malloc申请
				_memory = (char*)SystemAlloc(_reminSize);
				if (_memory == nullptr) {
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory;
			//当T类型小于一个指针的大小时，为了满足自由链表存储指针的需求，返回指针的大小
			objSize = objSize < sizeof(void*) ? sizeof(void*) : sizeof(T);
			
			//内存块起始位置后移
			_memory += objSize;

			//更新剩余内存块大小
			_reminSize -= objSize;
		}

		//定位new 用来初始化已有空间，调用自定义类型的构造函数
		new(obj)T;

		return obj;
	}

	void Delete(T* obj) {
		//显示调用析构函数
		obj->~T();

		//头插
		//obj的前4/8个字节用来存储当前的第一个起始位置
		//这样写无论是32位还是64位平台，都可以自动适配指针的大小
		*(void**)obj = _freeList;
		//更新自由链表的起始位置
		_freeList = obj;
	}

private:
	//缺省值
	char* _memory = nullptr;//申请的大块内存
	void* _freeList = nullptr;//释放的链表
	size_t _reminSize = 0;//剩余内存块大小
};



//测试定长内存池性能
struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{

	//验证还回来的内存是否重复利用的问题
	ObjectPool<TreeNode> tnPool;
	TreeNode* node1 = tnPool.New();
	TreeNode* node2 = tnPool.New();
	cout << node1 << endl;
	cout << node2 << endl;

	tnPool.Delete(node1);
	TreeNode* node3 = tnPool.New();
	cout << node3 << endl;

	cout << endl;

	//验证内存池到底快不快，有没有做到性能的优化

	const size_t Rounds = 10;//申请释放的轮次
	const size_t N = 100000;//每轮申请释放的次数

	std::vector<TreeNode*> v1;
	v1.reserve(N);

#ifdef _WIN32
	size_t begin1 = clock();
#else
	struct timeval begin1;
	gettimeofday(&begin1, nullptr);
#endif

	for (size_t i = 0; i < Rounds; ++i) {
		for (size_t i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (size_t i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}

#ifdef _WIN32
	size_t end1 = clock();
#else
	struct timeval end1;
	gettimeofday(&end1, nullptr);
#endif


	//这里我们调用自己所写的内存池
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	ObjectPool<TreeNode> tnPool1;

#ifdef _WIN32
	size_t begin2 = clock();
#else
	struct timeval begin2;
	gettimeofday(&begin2, nullptr);
#endif
	for (size_t i = 0; i < Rounds; ++i) {
		for (size_t i = 0; i < N; ++i)
		{
			v2.push_back(tnPool1.New());
		}
		for (size_t i = 0; i < N; ++i)
		{
			tnPool1.Delete(v2[i]);
		}

		v2.clear();
	}

#ifdef _WIN32
	size_t end2 = clock();

	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pool cost time:" << end2 - begin2 << endl;
#else
	//timeval结构体定义
	/*
	struct  timeval {
		long  tv_sec;  //秒
		long  tv_usec; //微妙
	};
	*/

	struct timeval end2;
	gettimeofday(&end2, nullptr);

	//先将秒转微秒 再+不足1秒的微秒 = 总运行微秒数，再/1000换算为毫秒
	double costTime1 = ((end1.tv_sec - begin1.tv_sec) * 1000000 + (end1.tv_usec - begin1.tv_usec)) / 1000;
	double costTime2 = ((end2.tv_sec - begin2.tv_sec) * 1000000 + (end2.tv_usec - begin2.tv_usec)) / 1000;
	
	cout << "new cost time:" << costTime1 << endl;
	cout << "object pool cost time:" << costTime2 << endl;
#endif	 
}