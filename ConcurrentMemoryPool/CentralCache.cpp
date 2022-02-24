#include "CentralCache.h"
#include "PageCache.h"

CentralCache CentralCache::_instance;

Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size) {
	//遍历当前链表找到一个可以使用的span
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList) {
			return it;
		}
		it = it->_next;
	}

	PageCache::GetInstance()->_mtx.lock();
	//没有可使用的span则向PageCache申请页
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
	//将该span标注为使用，防止其他线程合并了
	span->_isUse = true;
	//更新该span切分的大小
	span->_objectSize = byte_size;
	PageCache::GetInstance()->_mtx.unlock();

	//将申请到的大块内存切分为_freeList小块内存

	//char*方便+；大块内存的起始地址 = 起始页*页大小
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	//大块内存的结束地址 = 起始地址+span的大小（维护的页数*页大小）
	char* end = start + (span->_n << PAGE_SHIFT);

	//_freeList = 起始页 然后每byte_size连接下一个地址
	span->_freeList = start;
	//start+的过程中可能会跳过end，总大小可能并不能整除小块内存
	while (start + byte_size < end) {
		NextObj(start) = start + byte_size;
		start += byte_size;
	}

	//最后一个置空
	NextObj(start) = nullptr;
	
	//将申请好的span插入链表中
	list.PushFront(span);

	return span;
}

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t byte_size) {
	//根据大小查找是SpanList的第几个span对象
	size_t index = SizeClass::Index(byte_size);

	//加锁
	//不是同一把锁，锁的就不是同一个资源
	//这个锁不是全局锁，每个链表有一个，不是同一把锁，其他线程获取的是其他链表里的锁，所以不影响
	//假如线程1获取到_spanList[1]的锁，发现没有加锁，然后加锁执行
	//线程2执行到这里获取的是_spanList[3]的锁，发现没有加锁，然后加锁执行
	//线程3执行到这里获取的是_spanList[1]的锁，发现被加锁了，则阻塞等待_spanList[1]的锁解锁释放
	_spanLists[index]._mtx.lock();

	//获取Span对象
	Span* span = GetOneSpan(_spanLists[index],byte_size);
	//至少得有一个span
	assert(span);
	//span的内存块没用完
	assert(span->_freeList);

	//从span中获取batchNum个对象，将这批内存块从span自由链表中移除
	//不够batchNum个，则有多少拿多少
	start = span->_freeList;
	end = start;

	//从1开始，因为已经拿到了1个
	size_t actualNum = 1;
	
	//拿到一个区间的内存
	size_t i = 0;
	while (i < batchNum - 1 && NextObj(end)) {
		end = NextObj(end);
		++i;
		++actualNum;
	}

	span->_freeList = NextObj(end);
	
	//拿走的尾指向空
	NextObj(end) = nullptr;

	//增加已经使用的个数
	span->_useCount += actualNum;

	//解锁
	_spanLists[index]._mtx.unlock();

	return actualNum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t byte_size) {
	
	//先根据大小计算出是哪一个链表，将其加锁
	size_t index = SizeClass::Index(byte_size);
	_spanLists[index]._mtx.lock();

	while (start) {
		void* next = NextObj(start);
		
		//根据当前指针查找出属于的span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//将其插入到span的_freeList中
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		
		//使用计数减少，==0时说明该分配的所有的小块内存全部收回
		--(span->_useCount);
		if (span->_useCount == 0) {
			//从当前链表移除
			_spanLists[index].Erase(span);

			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//解锁 去PageCache回收时，解锁让其他线程也可以来释放
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_mtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_mtx.unlock();

			_spanLists[index]._mtx.lock();

		}

		//继续下一个
		start = next;
	}

	_spanLists[index]._mtx.unlock();
}
