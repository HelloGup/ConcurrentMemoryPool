#pragma once
#include "Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_instance;
	}

	//��ȡһ��pageNumҳ��span
	Span* NewSpan(size_t pageNum);

	//��ȡҳ�Ŷ�Ӧ��Span
	Span* MapObjectToSpan(void* ptr);

	//�ͷſ��е�Span��PageCache,���ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

	std::recursive_mutex _mtx;//��ʹ��Ͱ��

private:
	//ֱ�Ӷ�ֵ����ϣͰ 0���ã�����129ҳ
	SpanList _spanLists[PAGE_LIST_SIZE];

	std::unordered_map<PAGE_ID, Span*> _idSpanMap;

	PageCache() {};
	PageCache(const PageCache& pageObj) = delete;

	static PageCache _instance;
};