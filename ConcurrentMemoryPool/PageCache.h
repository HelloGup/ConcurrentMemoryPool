#pragma once
#include "Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_instance;
	}

	//��ȡһ��pageNumҳ��span
	Span* NewSpan(size_t pageNum);

	std::recursive_mutex _mtx;//��ʹ��Ͱ��

private:
	//ֱ�Ӷ�ֵ����ϣͰ 0���ã�����129ҳ
	SpanList _spanLists[PAGE_LIST_SIZE];


	PageCache() {};
	PageCache(const PageCache& pageObj) = delete;

	static PageCache _instance;
};