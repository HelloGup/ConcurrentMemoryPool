#include "PageCache.h"

PageCache PageCache::_instance;

Span* PageCache::NewSpan(size_t pageNum) {
	assert(pageNum > 0 && pageNum < PAGE_LIST_SIZE);

	//因为这里有一个递归调用，但是原先的锁资源的生命周期还没有到，应该使用递归锁
	std::lock_guard<std::recursive_mutex> lock(PageCache::GetInstance()->_mtx);

	//如果当前桶里有可用的大块内存
	if (!_spanLists[pageNum].Empty()) {
		//返回并从链表中删除
		return _spanLists[pageNum].PopFront();
	}

	//检查后面的桶里有没有大块内存，如果有将它切分
	//pageNum+1：从当前的下一个开始检查
	for (size_t i = pageNum + 1; i < PAGE_LIST_SIZE; ++i) {
		//找到就切分
		if (!_spanLists[i].Empty()) {
			//拿到一个大块内存span
			Span* n_span = _spanLists[i].PopFront();
			
			//在n_span的头部切出一个pageNum页的span
			Span* pageNum_span = new Span;
			//pageNum_span的起始页=n_span的起始页，页数就是需要的页数page_num;
			pageNum_span->_pageId = n_span->_pageId;
			pageNum_span->_n = pageNum;

			//更新n_span的页数和起始页
			n_span->_pageId += pageNum;//向后增加pageNum页
			n_span->_n -= pageNum;//页数减少了pageNum页

			//切剩下的插入对应链表里
			_spanLists[n_span->_n].PushFront(n_span);

			return pageNum_span;
		}

	}

	//到这里说明pageNum往后都没有大块内存了
	//这时候就在堆里申请一个128页的大块内存
	Span* bigPage = new Span;
	void* ptr = SystemAlloc((PAGE_LIST_SIZE - 1) << PAGE_SHIFT);

	//更新起始页和页数
	bigPage->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigPage->_n = PAGE_LIST_SIZE - 1;

	//申请到插入对应链表里
	_spanLists[bigPage->_n].PushFront(bigPage);

	//递归调用，不用再写一遍切分
	return NewSpan(pageNum);//注意如果加锁要加递归锁，防止互斥锁死锁
}