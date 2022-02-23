#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

//��̬ȫ�ֵ�ǰ�ļ��ɼ�
static void* ConcurrentAlloc(size_t size) {
	//��������Ǵ���256k������PageCache������������䣨32ҳ < ��Ҫ���ڴ� <= 128ҳ�������Χ���ڴ�
	if (size > MAX_BYTES) {
		//�������Ҫ��ҳ��
		size_t ret_size = SizeClass::RoundUp(size);
		size_t page = ret_size >> PAGE_SHIFT;

		//����ҳ����ȡspan
		PageCache::GetInstance()->_mtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(page);
		PageCache::GetInstance()->_mtx.unlock();

		//�������뵽��span����ʼ��ַ
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);

		return ptr;
	}
	else {
		//ͨ��TLSÿ���߳�������ȡ�Լ�ר����ThreadCache
		if (pTLSThreadCache == nullptr) {
			//ʹ�ö����ڴ�ش���
			static ObjectPool<ThreadCache> threadCachePool;
			pTLSThreadCache = threadCachePool.New();
		}

		//����ÿ���߳��Ƿ����һ��ThreadCache
		//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr,size_t byte_size) {

	if (byte_size > MAX_BYTES) {
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);

		PageCache::GetInstance()->_mtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_mtx.unlock();


	}
	else {
		//�ͷ�ʱÿ���߳�Ҫ�Ѿ��и��ԵĶ���
		assert(pTLSThreadCache);

		pTLSThreadCache->Deallocate(ptr, byte_size);
	}
}