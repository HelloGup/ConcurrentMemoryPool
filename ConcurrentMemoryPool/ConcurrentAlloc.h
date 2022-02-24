#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//静态全局当前文件可见
static void* ConcurrentAlloc(size_t byte_size) {
	//有种情况是大于256k，但是PageCache还可以满足分配（32页 < 需要的内存 <= 128页）这个范围的内存
	if (byte_size > MAX_BYTES) {
		//计算出需要的页数
		size_t ret_size = SizeClass::RoundUp(byte_size);
		size_t page = ret_size >> PAGE_SHIFT;

		//根据页数获取span
		PageCache::GetInstance()->_mtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(page);
		span->_objectSize = ret_size;
		PageCache::GetInstance()->_mtx.unlock();

		//返回申请到的span的起始地址
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);

		return ptr;
	}
	else {
		//通过TLS每个线程无锁获取自己专属的ThreadCache
		if (pTLSThreadCache == nullptr) {
			//使用定长内存池创建
			static ObjectPool<ThreadCache> threadCachePool;
			pTLSThreadCache = threadCachePool.New();
		}

		//测试每个线程是否独享一份ThreadCache
		//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

		return pTLSThreadCache->Allocate(byte_size);
	}
}

static void ConcurrentFree(void* ptr) {

	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t byte_size = span->_objectSize;

	//大于可申请的最大字节数直接找PageCache回收
	if (byte_size > MAX_BYTES) {
		PageCache::GetInstance()->_mtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_mtx.unlock();
	}
	else {
		//释放时每个线程要已经有各自的对象
		assert(pTLSThreadCache);

		pTLSThreadCache->Deallocate(ptr, byte_size);
	}
}