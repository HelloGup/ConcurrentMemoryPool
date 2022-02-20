#pragma once
#include "Common.h"


//һ������ֻ��һ�����Ļ��棬��ô����ʹ�õ���ģʽ
class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_instance;
	}

	//��SpanList��ȡһ���ǿյ�Span����
	Span* GetOneSpan(SpanList& list, size_t byte_size);

	//�����Ļ����ȡһ�������Ķ����thread_cache,����ʵ�ʵ�����
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum/*��Ҫ������*/, size_t byte_size);

	
private:
	CentralCache() {}

	CentralCache(const CentralCache&) = delete;

private:
	SpanList _spanLists[FREE_LIST_SIZE];//��thread cache�Ķ������һ��������һ��
	
	//��̬���ݳ�Ա�����ǲ���ȫ����
	//���Ǿ�̬���ݳ�Ա���ܵ����ƣ�ֻ���������������ָ�������
	static CentralCache _instance;//����ֻ��������û�ж���
};
