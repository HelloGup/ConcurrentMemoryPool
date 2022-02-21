#pragma once
#include "Common.h"
#include "CentralCache.h"

class ThreadCache {

public:
	//������ͷ��ڴ����
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	//����һ�����Ļ����ȡ�ڴ�
	void* FetchFromCentralCache(size_t index, size_t size);
private:
	FreeList _freeLists[FREE_LIST_SIZE];
};

//�̱߳��ػ��洢(GCC������Ҫ���Ӷ�̬�� -lpthread)
static thread_local ThreadCache* pTLSThreadCache = nullptr;