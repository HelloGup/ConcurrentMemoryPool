#pragma once
#include "Common.h"
#include "CentralCache.h"

class ThreadCache {

public:
	//申请和释放内存对象
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	//从上一层中心缓存获取内存
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _freeLists[FREE_LIST_SIZE];
};

//线程本地化存储(GCC编译需要链接动态库 -lpthread)
static thread_local ThreadCache* pTLSThreadCache = nullptr;