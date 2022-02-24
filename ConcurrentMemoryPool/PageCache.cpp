#include "PageCache.h"

PageCache PageCache::_instance;

Span* PageCache::NewSpan(size_t pageNum) {
	assert(pageNum > 0);

	//大于128页的使用系统申请
	if (pageNum > PAGE_LIST_SIZE - 1) {
		void* ptr = SystemAlloc(pageNum << PAGE_SHIFT);
		
		//使用定长内存池new
		//Span* span = new Span;
		Span* span = _spanPool.New();
		//维护起始页和页数
		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = pageNum;

		//存入map
		//_idSpanMap[span->_pageId] = span;
		_idSpanMap.set(span->_pageId, span);

		return span;
	}

	//如果当前桶里有可用的大块内存
	if (!_spanLists[pageNum].Empty()) {
		//返回并从链表中删除
		//pageNum_span里面维护的每一页都映射自己 方便CentralCache回收查找span
		Span* span = _spanLists[pageNum].PopFront();
		for (PAGE_ID i = 0; i < span->_n; ++i) {
			//_idSpanMap[span->_pageId + i] = span;
			_idSpanMap.set(span->_pageId + i, span);
		}
		return span;
	}

	//检查后面的桶里有没有大块内存，如果有将它切分
	//pageNum+1：从当前的下一个开始检查
	for (size_t i = pageNum + 1; i < PAGE_LIST_SIZE; ++i) {
		//找到就切分
		if (!_spanLists[i].Empty()) {
			//拿到一个大块内存span
			Span* n_span = _spanLists[i].PopFront();
			
			//在n_span的头部切出一个pageNum页的span
			//Span* pageNum_span = new Span;
			Span* pageNum_span = _spanPool.New();

			//pageNum_span的起始页=n_span的起始页，页数就是需要的页数page_num;
			pageNum_span->_pageId = n_span->_pageId;
			pageNum_span->_n = pageNum;

			//更新n_span的页数和起始页
			n_span->_pageId += pageNum;//向后增加pageNum页
			n_span->_n -= pageNum;//页数减少了pageNum页

			//切剩下的插入对应链表里
			_spanLists[n_span->_n].PushFront(n_span);

			//将n_span的首尾页映射自己，方便PageCahe回收时合并进行查找
			//_idSpanMap[n_span->_pageId] = n_span;
			_idSpanMap.set(n_span->_pageId, n_span);
			//注意区间要-1 因为起始页就是1页
			//_idSpanMap[n_span->_pageId + n_span->_n - 1] = n_span;
			_idSpanMap.set(n_span->_pageId + n_span->_n - 1, n_span);

			//pageNum_span里面维护的每一页都映射自己 方便CentralCache回收查找span
			for (PAGE_ID i = 0; i < pageNum_span->_n; ++i) {
				//_idSpanMap[pageNum_span->_pageId + i] = pageNum_span;
				_idSpanMap.set(pageNum_span->_pageId + i, pageNum_span);
			}

			return pageNum_span;
		}

	}

	//到这里说明pageNum往后都没有大块内存了
	//这时候就在堆里申请一个128页的大块内存
	//Span* bigPage = new Span;
	Span* bigPage = _spanPool.New();

	void* ptr = SystemAlloc((PAGE_LIST_SIZE - 1) << PAGE_SHIFT);

	//更新起始页和页数
	bigPage->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigPage->_n = PAGE_LIST_SIZE - 1;

	//申请到插入对应链表里
	_spanLists[bigPage->_n].PushFront(bigPage);

	//递归调用，不用再写一遍切分
	return NewSpan(pageNum);//注意如果加锁要加递归锁，防止互斥锁死锁,这里是在外部调用处加锁解决
}

//获取页号对应的Span
Span* PageCache::MapObjectToSpan(void* ptr) {
	//RAII
	//加锁避免修改map时，其他线程读出错，需要注意不要与外部调用NewSpan时加锁两次产生死锁
	//使用基数树不需要加锁了
	/*std::unique_lock<std::mutex> lock(_mtx);

	auto it = _idSpanMap.find((PAGE_ID)ptr >> PAGE_SHIFT);
	if (it != _idSpanMap.end()) {
		return it->second;
	}
	else {
		assert(false);
		return nullptr;
	}*/

	//为什么不用加锁了
	//基数树在一开始就开好了空间，并且在读写时不会动结构不需要加锁，而红黑树旋转、哈希扩容都会改变原结构需要加锁
	//而加锁解锁牺牲了极大的效率
	auto ret = (Span*)_idSpanMap.get((PAGE_ID)ptr >> PAGE_SHIFT);
	assert(ret);
	return ret;
}

//释放空闲的Span到PageCache,并合并相邻的span
void PageCache::ReleaseSpanToPageCache(Span* span) {
	//对span前后的页，尝试进行合并，缓解内存碎片的问题
	
	//如果是大于128页的，是向系统申请的，直接还给堆
	if (span->_n > PAGE_LIST_SIZE - 1) {
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);

		//使用定长内存池释放
		//delete span;
		_spanPool.Delete(span);
		return;
	}
	
	//向前合并
	while (true) {
		//可以合并的前一个span的起始页号
		PAGE_ID prevId = span->_pageId - 1;
		//auto it = _idSpanMap.find(prevId);
		//
		////前面的页号没有span，退出
		//if (it == _idSpanMap.end()) {
		//	break;
		//}
		//使用基数树
		auto ret = (Span*)_idSpanMap.get(prevId);
		if (ret == nullptr) {
			break;
		}

		//前面的页号span还在使用，退出
		/*Span* prev = it->second;
		if (prev->_isUse == true) {
			break;
		}*/
		if (ret->_isUse) {
			break;
		}

		//合并后页数大于PageCache管理的最大页，也没办法合并，退出
		/*if (prev->_n + span->_n > PAGE_LIST_SIZE - 1) {
			break;
		}*/
		if (ret->_n + span->_n > PAGE_LIST_SIZE - 1) {
			break;
		}

		//到这里说明可以合并
		//span->_pageId = prev->_pageId;
		//span->_n += prev->_n;
		span->_pageId = ret->_pageId;
		span->_n += ret->_n;

		//前一个被合并后将其从链表中移除
		//_spanLists[prev->_n].Erase(prev);
		_spanLists[ret->_n].Erase(ret);

		//delete prev;
		//_spanPool.Delete(prev);
		_spanPool.Delete(ret);
	}

	//向后合并
	while (true) {
		//可以合并的后一个span的起始页号
		PAGE_ID nextId = span->_pageId + span->_n;
		/*auto it = _idSpanMap.find(nextId);
		if (it == _idSpanMap.end()) {
			break;
		}*/
		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr) {
			break;
		}

		/*Span* next = it->second;
		if (next->_isUse) {
			break;
		}*/
		if (ret->_isUse) {
			break;
		}

		/*if (span->_n + next->_n > PAGE_LIST_SIZE - 1) {
			break;
		}*/
		if (span->_n + ret->_n > PAGE_LIST_SIZE - 1) {
			break;
		}

		//向后合并起始页不用变
		//span->_n += next->_n;
		span->_n += ret->_n;

		//_spanLists[next->_n].Erase(next);
		_spanLists[ret->_n].Erase(ret);
		
		//delete next;
		//_spanPool.Delete(next);
		_spanPool.Delete(ret);
	}

	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;
	
	//首尾页号都要映射
	/*_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
}