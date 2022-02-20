#pragma once
#include "Common.h"
#include "ThreadCache.h"

//��̬ȫ�ֵ�ǰ�ļ��ɼ�
static void* ConcurrentAlloc(size_t size) {
	//ͨ��TLSÿ���߳�������ȡ�Լ�ר����ThreadCache
	if (pTLSThreadCache == nullptr) {
		pTLSThreadCache = new ThreadCache;
	}

	//����ÿ���߳��Ƿ����һ��ThreadCache
	//cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;

	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr,size_t size) {
	//�ͷ�ʱÿ���߳�Ҫ�Ѿ��и��ԵĻ���
	assert(pTLSThreadCache);

	pTLSThreadCache->Deallocate(ptr,size);
}