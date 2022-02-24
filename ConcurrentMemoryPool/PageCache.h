#pragma once
#include "Common.h"
#include "objectPool.h"
#include "PageMap.h"

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

	std::mutex _mtx;//不使用桶锁

private:
	//直接定值法哈希桶 0不用，最大给129页
	SpanList _spanLists[PAGE_LIST_SIZE];

	ObjectPool<Span> _spanPool;

	//使用基数树脱离map，map内部使用了malloc，并且存在锁竞争，降低效率
	
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	PageCache() {};
	PageCache(const PageCache& pageObj) = delete;

	static PageCache _instance;
};