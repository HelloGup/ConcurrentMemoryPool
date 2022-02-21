#include "ThreadCache.h"

void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);

	//�����������ڴ�
	size_t alignSize = SizeClass::RoundUp(size);
	//���������ڴ���ڹ�ϣ���ĸ�����
	size_t index = SizeClass::Index(size);

	//��������Ϊ�գ���ʹ������������ڴ��
	if (!_freeLists[index].Empty()) {
		return _freeLists[index].pop();
	}
	else {//��������Ϊ�գ������ϲ������ڴ�
		return FetchFromCentralCache(index, alignSize);
	}
}

void ListTooLong(FreeList& list, size_t size) {
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());

	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}

void ThreadCache::Deallocate(void* ptr, size_t size/*���ݴ�Сȷ������ڴ�����ĸ�Ͱ��*/) {
	assert(size <= MAX_BYTES);//����256k��Ӧ���������ͷ�
	assert(ptr);

	//�Ҷ�Ӧӳ�����������Ͱ�������ȥ
	size_t index = SizeClass::Index(size);
	_freeLists[index].push(ptr);

	//�����ȴ���һ����������ľͿ�ʼ���գ�Ҳ�������Ӹ���_freeList�Ĵ�С�����գ�
	//�����ڴ�  ֻ�ǻ��������һ����֮ǰ�����û���գ�������ȫ������
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize()) {
		ListTooLong(_freeLists[index],size);
	}
}



void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//...
	//����ʼ���������㷨
	//1.�ʼ����һ����central cacheҪ̫�࣬Ҫ̫������ò���
	//��׼���е�min��max��<windows.h>�д�ͳ��min/max�궨���г�ͻ��common���ô����
	size_t batchNum = std::min(_freeLists[index].MaxSize(),SizeClass::NumMoveSize(size));

	//2.���������Ҫsize��С�ڴ��������ôbatNum�ͻ᲻��������ֱ������
	//sizeԽ��һ��Ҫ��Խ��
	if (_freeLists[index].MaxSize() == batchNum) {
		_freeLists[index].MaxSize() += 1;//���+1��������������+2...
	}

	//һ�θ��������ҪֻҪһ����Χ�����忪ʼ�ͽ���ָ��
	void* start = nullptr;
	void* end = nullptr;
	//ҪbatNum������span��һ������ô��������صĲ���ʵ���õ���
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);//����Ҫ��һ��

	//���ֻ��һ������ôstart��end��ͬ
	if (actualNum == 1) {
		assert(start == end);
		return start;
	}
	else {
		//startҪ���أ���start����һ����ʼ������������ 
		_freeLists[index].PushRange(NextObj(start), end, actualNum - 1/*һ���Ѿ�ʹ����*/);
		return start;
	}
}