#pragma once
#include "Common.h"


//一个进程只有一个中心缓存，那么可以使用单例模式
class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_instance;
	}

	//从SpanList获取一个非空的Span对象
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	//从中心缓存获取一定数量的对象给thread_cache,返回实际的数量
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum/*需要的数量*/, size_t byte_size);

	
private:
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;

private:
	SpanList _spanLists[FREE_LIST_SIZE];//和thread cache的对齐规则一样，长度一样
	
	//静态数据成员可以是不完全类型
	//而非静态数据成员则受到限制，只能声明它所属类的指针或引用
	static CentralCache _instance;//类中只是声明，没有定义
};
