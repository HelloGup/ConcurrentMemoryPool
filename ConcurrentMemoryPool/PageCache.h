#pragma once
#include "Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_instance;
	}

	//获取一个pageNum页的span
	Span* NewSpan(size_t pageNum);

	//获取页号对应的Span
	Span* MapObjectToSpan(void* ptr);

	//释放空闲的Span到PageCache,并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

	std::recursive_mutex _mtx;//不使用桶锁

private:
	//直接定值法哈希桶 0不用，最大给129页
	SpanList _spanLists[PAGE_LIST_SIZE];

	std::unordered_map<PAGE_ID, Span*> _idSpanMap;

	PageCache() {};
	PageCache(const PageCache& pageObj) = delete;

	static PageCache _instance;
};