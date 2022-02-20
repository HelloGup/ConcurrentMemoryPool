#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_instance;

Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size) {
	//������ǰ�����ҵ�һ������ʹ�õ�span
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList) {
			return it;
		}
		it = it->_next;
	}

	//û�п�ʹ�õ�span����PageCache����ҳ
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));

	//�����뵽�Ĵ���ڴ��з�Ϊ_freeListС���ڴ�
	//char*����+������ڴ����ʼ��ַ = ��ʼҳ*ҳ��С
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	//����ڴ�Ľ�����ַ = ��ʼ��ַ+span�Ĵ�С��ά����ҳ��*ҳ��С��
	char* end = start + (span->_n << PAGE_SHIFT);

	//_freeList = ��ʼҳ Ȼ��ÿbyte_size������һ����ַ
	span->_freeList = start;
	while (start + byte_size != end) {
		NextObj(start) = start + byte_size;
		start += byte_size;
	}

	//���һ���ÿ�
	NextObj(start) = nullptr;
	
	//������õ�span����������
	list.PushFront(span);

	return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t byte_size) {
	//���ݴ�С������SpanList�ĵڼ���span����
	size_t index = SizeClass::Index(byte_size);

	//����
	//����ͬһ���������ľͲ���ͬһ����Դ
	//���������ȫ������ÿ��������һ��������ͬһ�����������̻߳�ȡ����������������������Բ�Ӱ��
	//�����߳�1��ȡ��_spanList[1]����������û�м�����Ȼ�����ִ��
	//�߳�2ִ�е������ȡ����_spanList[3]����������û�м�����Ȼ�����ִ��
	//�߳�3ִ�е������ȡ����_spanList[1]���������ֱ������ˣ��������ȴ�_spanList[1]���������ͷ�
	_spanLists[index]._mtx.lock();

	//��ȡSpan����
	Span* span = GetOneSpan(_spanLists[index],byte_size);
	//���ٵ���һ��span
	assert(span);
	//span���ڴ��û����
	assert(span->_freeList);

	//��span�л�ȡbatchNum�����󣬽������ڴ���span�����������Ƴ�
	//����batchNum�������ж����ö���
	start = span->_freeList;
	end = start;

	//��1��ʼ����Ϊ�Ѿ��õ���1��
	size_t actualNum = 1;
	
	//�õ�һ��������ڴ�
	size_t i = 0;
	while (i < batchNum - 1 && NextObj(end)) {
		end = NextObj(end);
		++i;
		++actualNum;
	}

	span->_freeList = NextObj(end);
	
	//���ߵ�βָ���
	NextObj(end) = nullptr;

	//����
	_spanLists[index]._mtx.unlock();

	return actualNum;
}
