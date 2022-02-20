#pragma once
#include <iostream>
#include <vector>
#include <assert.h>
#include <thread>
#include <mutex>
#include <algorithm>

//标准库中的min和max与<windows.h>中传统的min/max宏定义有冲突
//在引用<windows.h>之前#define NOMINMAX，可以禁用windows.h中的min和max
#define NOMINMAX
#include <Windows.h>

#ifdef _WIN32
#include <Windows.h>//VirtualAlloc()
#else
#include <unistd.h> //sbrk()
#include <sys/time.h> //linux下使用gettimeofday()函数计时
#endif

using std::cout;
using std::endl;

//最大只申请256kb内存
static const size_t MAX_BYTES = 256 * 1024;
//哈希桶的最大数量
static const size_t FREE_LIST_SIZE = 208;
//PageCache里的哈希桶数量，几页就映射哪个索引 满足可以开4个最大内存
static const size_t PAGE_LIST_SIZE = 129;
//页的大小为8k，2^13
static const size_t PAGE_SHIFT = 13;

inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#else
	//Linux下brk/sbrk/mmap等
	//https://www.cnblogs.com/vinozly/p/5489138.html

	// int brk(void *addr);
	// 这个函数的参数是一个地址，假如你已经知道了堆的起始地址，还有堆的大小
	// 那么你就可以据此修改 brk() 中的地址参数已达到调整堆的目的。

	//void* sbrk(intptr_t increment);
	/*当 increment 为正时，则按 increment 的大小(字节)，开辟内存空间，并返回开辟前，程序中断点（program break）的地址。
	  当 increment 为负时，则按 increment 的大小(字节)，释放内存空间，并返回释放前，程序中断点的地址。
	  当 increment 为零时，不会分配或释放空间，返回当前程序中断点的地址。
	*/

	void* ptr = sbrk(kpage);

#endif
	if (ptr == nullptr) {
		throw std::bad_alloc();
	}
	return ptr;
}

//返回引用可以更改
//在.h中定义了函数及其实现，如被包含时则会把函数实现放入包含的位置，被包含多次时会被放入多次，导致函数重定义
//使用static修饰，因为对于静态函数、类、namespace，无论有多少文件包含它，也只会维护一份，链接时也就只能找到这一份，所以也是没有问题
static void*& NextObj(void* obj) {
	return *(void**)obj;
}

class FreeList {
public:
	void push(void* obj) {
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
	}

	//插入一个范围
	void PushRange(void* start, void* end) {
		NextObj(end) = _freeList;
		_freeList = start;
	}

	void* pop() {
		assert(_freeList);
		void* obj = _freeList;//这里他返回错了
		void* next = NextObj(_freeList);
		_freeList = next;

		return obj;
	}

	bool Empty() {
		return _freeList == nullptr;
	}

	//传引用外部可以赋值
	size_t& MaxSize() {
		return _maxSize;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;//慢开始阈值
};


//计算对象大小的对齐映射规则，管理对齐和映射等关系
class SizeClass {
public:
	//整体控制在最多11%左右的内碎片浪费
	//最少要8字节，因64位下存储指针就需要8字节
	//【1，128】                8byte对齐        freeList[0,16)按每8字节递增对齐需要16个桶（0-15）128/8=16
	//【128+1，1024】           16byte对齐       freeList[16,72)按每16字节递增对齐需要56个桶（16-71）(1024-128)/16=56
	//【1024+1，8*1024】        128byte对齐      freeList[72,128)
	//【8*1024+1，64*1024】     1024byte对齐     freeList[128,184)
	//【64*1024+1，256*1024】   8*1024byte对齐   freeList[184,208)

	//计算当前字节需要分配多少内存
	//普通写法
	//size_t _RoundUp(size_t bytes,size_t align/*对齐数*/) {
	//	size_t alignSize;
	//	if (bytes % align != 0) {
	//		alignSize = (bytes / align + 1) * align;
	//	}
	//	else {
	//		alignSize = bytes;
	//	}

	//	return alignSize;
	//}
	//高级写法
	static inline size_t _RoundUp(size_t bytes, size_t align/*对齐数*/) {
		//根据对齐数返回应分配的内存大小
		return ((bytes + align - 1) & ~(align - 1));
	}

	static inline size_t RoundUp(size_t bytes) {
		//根据字节大小，确定对齐数
		if (bytes <= 128) {
			return _RoundUp(bytes, 8);
		}
		else if (bytes <= 1024) {
			return _RoundUp(bytes, 16);
		}
		else if (bytes <= 8 * 1024) {
			return _RoundUp(bytes, 128);
		}
		else if (bytes <= 64 * 1024) {
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 256 * 1024) {
			return _RoundUp(bytes, 8 * 1024);
		}
		else {
			assert(false);
			return -1;
		}
	}

	//计算当前字节对应的自由链表中的下标
	//普通写法
	static inline size_t _Index(size_t bytes, size_t align) {
		if (bytes % align == 0) {
			return bytes / align - 1;
		}
		else {
			return bytes / align;
		}
	}

	//static inline size_t _Index(size_t bytes, size_t align_shift/*2的几次方*/) {
	//	return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	//}

	static inline size_t Index(size_t bytes) {
		assert(bytes <= MAX_BYTES);

		static int group_array[4] = { 16,56,56,56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[0] + group_array[1];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[0] + group_array[1] + group_array[2];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[0] + group_array[1] + group_array[2] + group_array[3];
		}
		else {
			assert(false);
		}
	}

	//一次thread_cache从中心缓存中获取多少个
	static size_t NumMoveSize(size_t byte_size/*单个对象的大小*/) {

		assert(byte_size > 0);

		//[2,512]:一次批量移动多少个对象的（慢启动）上下限值
		//目标：小对象给的多 大对象给的少
		//根据一个内存块可以有多少个单个对象，来判断给多少个
		int num = MAX_BYTES / byte_size;
		//最少给2个
		if (num < 2) {
			num = 2;
		}
		//最多给512个
		if (num > 512) {
			num = 512;
		}
		return num;
	}

	//计算需要多少页
	static size_t NumMovePage(size_t byte_size/*单个对象的大小*/) {
		//先计算出需要给多少个小块内存
		size_t batchNum = NumMoveSize(byte_size);

		//根据需要的小块内存大小算出需要多少页
		size_t page_num = batchNum * byte_size >> PAGE_SHIFT;
		
		//最少给一页
		if (page_num == 0) {
			page_num = 1;
		}

		return page_num;
	}
};

//64位平台下，size_t存储页数会溢出，因为64位平台下有2^64 / 2^13(8k为一页) = 2^51页，这么多页int存不下
#ifdef _WIN64   //因为32位平台下没有_WIN64,64位平台下 _WIN32和_WIN64都定义了，只有先判断_WIN64才能区分
	typedef unsigned long long PAGE_ID;
#elif _WIN32 
	typedef size_t PAGE_ID;
#elif __x86_64__
	//linux 64位
	typedef unsigned long long PAGE_ID;
#elif __i386__
	//linux 32位
	typedef size_t PAGE_ID;
#endif

//管理多个连续页的内存跨度结构
//需要设计为双向链表节点，因为当它维护的内存全部没有再使用的时候，要还给上层，从中间释放用双向链表更方便
struct Span {
	//没有构造函数自己进行初始化
	PAGE_ID _pageId = 0;//大块内存起始页的页号
	size_t _n = 0;//当前span里维护的页数量 

	//双向链表结构
	Span* _next = nullptr;
	Span* _prev = nullptr;

	size_t _useCount = 0;//使用计数 ==0说明所有内存都还回来了，没有thread cache在使用

	void* _freeList = nullptr;//切好的小块内存自由链表
};

//带头双向循环链表
class SpanList {
public:
	SpanList() {
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin() {
		return _head->_next;
	}

	Span* End() {
		return _head;
	}

	//头插
	void PushFront(Span* span) {
		Insert(_head->_next, span);
	}

	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;

		prev->_next = newSpan;
		newSpan->_prev = prev;

		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	//头删
	Span* PopFront() {
		Span* front = _head->_next;
		//Erase里并没有释放空间，只是剔除它
		Erase(front);
		return front;
	}

	void Erase(Span* pos) {
		assert(pos);
		//不能把头节点删了
		assert(pos != _head);

		Span* prev = pos->_prev;
		prev->_next = pos->_next;
		pos->_next->_prev = prev;

		//不需要delete这个节点，我们要还给上层
	}

	bool Empty() {
		return _head->_next == _head;
	}

private:
	Span* _head = nullptr;

public:
	std::mutex _mtx;//桶锁：中心缓存存在线程安全问题
};