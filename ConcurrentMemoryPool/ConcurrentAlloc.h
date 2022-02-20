#pragma once
#include "Common.h"
#include "ThreadCache.h"

//静态全局当前文件可见
static void* ConcurrentAlloc(size_t size) {
	//通过TLS每个线程无锁获取自己专属的ThreadCache
	if (pTLSThreadCache == nullptr) {
		pTLSThreadCache = new ThreadCache;
	}

	//测试每个线程是否独享一份ThreadCache
	//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr,size_t size) {
	//释放时每个线程要已经有各自的缓存
	assert(pTLSThreadCache);

	pTLSThreadCache->Deallocate(ptr,size);
}