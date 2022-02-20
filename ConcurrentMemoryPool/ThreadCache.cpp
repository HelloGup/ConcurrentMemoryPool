#include "ThreadCache.h"

void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);

	//计算分配多少内存
	size_t alignSize = SizeClass::RoundUp(size);
	//计算分配的内存块在哈希的哪个索引
	size_t index = SizeClass::Index(size);//这里讲课有问题

	//自由链表不为空，则使用自由链表的内存块
	if (!_freeLists[index].Empty()) {
		return _freeLists[index].pop();
	}
	else {//自由链表为空，则向上层申请内存
		return FetchFromCentralCache(index, alignSize);
	}
}
void ThreadCache::Deallocate(void* ptr, size_t size/*根据大小确定这个内存放在哪个桶里*/) {
	assert(size <= MAX_BYTES);//大于256k不应该来这里释放
	assert(ptr);

	//找对应映射的自由链表桶，插入进去
	size_t index = SizeClass::Index(size);
	_freeLists[index].push(ptr);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	//...
	//慢开始反馈调节算法
	//1.最开始不会一次向central cache要太多，要太多可能用不完
	//标准库中的min和max与<windows.h>中传统的min/max宏定义有冲突，common引用处解决
	size_t batchNum = std::min(_freeLists[index].MaxSize(),SizeClass::NumMoveSize(size));

	//2.如果不断有要size大小内存的需求，那么batNum就会不断增长，直到上限
	//size越大，一次要的越少
	if (_freeLists[index].MaxSize() == batchNum) {
		_freeLists[index].MaxSize() += 1;//如果+1增长的慢，可以+2...
	}

	//一次给多个，需要只要一个范围，定义开始和结束指针
	void* start = nullptr;
	void* end = nullptr;
	//要batNum个，但span不一定有这么多个，返回的才是实际拿到的
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);//至少要给一个

	//如果只给一个，那么start和end相同
	if (actualNum == 1) {
		assert(start == end);
		return start;
	}
	else {
		//start要返回，从start的下一个开始链进自由链表 
		_freeLists[index].PushRange(NextObj(start), end);
		return start;
	}

	return nullptr;
}