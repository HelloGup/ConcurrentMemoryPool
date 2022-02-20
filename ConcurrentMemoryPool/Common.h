#pragma once
#include <iostream>
#include <vector>
#include <assert.h>
#include <thread>
#include <mutex>
#include <algorithm>

//��׼���е�min��max��<windows.h>�д�ͳ��min/max�궨���г�ͻ
//������<windows.h>֮ǰ#define NOMINMAX�����Խ���windows.h�е�min��max
#define NOMINMAX
#include <Windows.h>

#ifdef _WIN32
#include <Windows.h>//VirtualAlloc()
#else
#include <unistd.h> //sbrk()
#include <sys/time.h> //linux��ʹ��gettimeofday()������ʱ
#endif

using std::cout;
using std::endl;

//���ֻ����256kb�ڴ�
static const size_t MAX_BYTES = 256 * 1024;
//��ϣͰ���������
static const size_t FREE_LIST_SIZE = 208;
//PageCache��Ĺ�ϣͰ��������ҳ��ӳ���ĸ����� ������Կ�4������ڴ�
static const size_t PAGE_LIST_SIZE = 129;
//ҳ�Ĵ�СΪ8k��2^13
static const size_t PAGE_SHIFT = 13;

inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

#else
	//Linux��brk/sbrk/mmap��
	//https://www.cnblogs.com/vinozly/p/5489138.html

	// int brk(void *addr);
	// ��������Ĳ�����һ����ַ���������Ѿ�֪���˶ѵ���ʼ��ַ�����жѵĴ�С
	// ��ô��Ϳ��Ծݴ��޸� brk() �еĵ�ַ�����Ѵﵽ�����ѵ�Ŀ�ġ�

	//void* sbrk(intptr_t increment);
	/*�� increment Ϊ��ʱ���� increment �Ĵ�С(�ֽ�)�������ڴ�ռ䣬�����ؿ���ǰ�������жϵ㣨program break���ĵ�ַ��
	  �� increment Ϊ��ʱ���� increment �Ĵ�С(�ֽ�)���ͷ��ڴ�ռ䣬�������ͷ�ǰ�������жϵ�ĵ�ַ��
	  �� increment Ϊ��ʱ�����������ͷſռ䣬���ص�ǰ�����жϵ�ĵ�ַ��
	*/

	void* ptr = sbrk(kpage);

#endif
	if (ptr == nullptr) {
		throw std::bad_alloc();
	}
	return ptr;
}

//�������ÿ��Ը���
//��.h�ж����˺�������ʵ�֣��类����ʱ���Ѻ���ʵ�ַ��������λ�ã����������ʱ�ᱻ�����Σ����º����ض���
//ʹ��static���Σ���Ϊ���ھ�̬�������ࡢnamespace�������ж����ļ���������Ҳֻ��ά��һ�ݣ�����ʱҲ��ֻ���ҵ���һ�ݣ�����Ҳ��û������
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

	//����һ����Χ
	void PushRange(void* start, void* end) {
		NextObj(end) = _freeList;
		_freeList = start;
	}

	void* pop() {
		assert(_freeList);
		void* obj = _freeList;//���������ش���
		void* next = NextObj(_freeList);
		_freeList = next;

		return obj;
	}

	bool Empty() {
		return _freeList == nullptr;
	}

	//�������ⲿ���Ը�ֵ
	size_t& MaxSize() {
		return _maxSize;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;//����ʼ��ֵ
};


//��������С�Ķ���ӳ����򣬹�������ӳ��ȹ�ϵ
class SizeClass {
public:
	//������������11%���ҵ�����Ƭ�˷�
	//����Ҫ8�ֽڣ���64λ�´洢ָ�����Ҫ8�ֽ�
	//��1��128��                8byte����        freeList[0,16)��ÿ8�ֽڵ���������Ҫ16��Ͱ��0-15��128/8=16
	//��128+1��1024��           16byte����       freeList[16,72)��ÿ16�ֽڵ���������Ҫ56��Ͱ��16-71��(1024-128)/16=56
	//��1024+1��8*1024��        128byte����      freeList[72,128)
	//��8*1024+1��64*1024��     1024byte����     freeList[128,184)
	//��64*1024+1��256*1024��   8*1024byte����   freeList[184,208)

	//���㵱ǰ�ֽ���Ҫ��������ڴ�
	//��ͨд��
	//size_t _RoundUp(size_t bytes,size_t align/*������*/) {
	//	size_t alignSize;
	//	if (bytes % align != 0) {
	//		alignSize = (bytes / align + 1) * align;
	//	}
	//	else {
	//		alignSize = bytes;
	//	}

	//	return alignSize;
	//}
	//�߼�д��
	static inline size_t _RoundUp(size_t bytes, size_t align/*������*/) {
		//���ݶ���������Ӧ������ڴ��С
		return ((bytes + align - 1) & ~(align - 1));
	}

	static inline size_t RoundUp(size_t bytes) {
		//�����ֽڴ�С��ȷ��������
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

	//���㵱ǰ�ֽڶ�Ӧ�����������е��±�
	//��ͨд��
	static inline size_t _Index(size_t bytes, size_t align) {
		if (bytes % align == 0) {
			return bytes / align - 1;
		}
		else {
			return bytes / align;
		}
	}

	//static inline size_t _Index(size_t bytes, size_t align_shift/*2�ļ��η�*/) {
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

	//һ��thread_cache�����Ļ����л�ȡ���ٸ�
	static size_t NumMoveSize(size_t byte_size/*��������Ĵ�С*/) {

		assert(byte_size > 0);

		//[2,512]:һ�������ƶ����ٸ�����ģ���������������ֵ
		//Ŀ�꣺С������Ķ� ����������
		//����һ���ڴ������ж��ٸ������������жϸ����ٸ�
		int num = MAX_BYTES / byte_size;
		//���ٸ�2��
		if (num < 2) {
			num = 2;
		}
		//����512��
		if (num > 512) {
			num = 512;
		}
		return num;
	}

	//������Ҫ����ҳ
	static size_t NumMovePage(size_t byte_size/*��������Ĵ�С*/) {
		//�ȼ������Ҫ�����ٸ�С���ڴ�
		size_t batchNum = NumMoveSize(byte_size);

		//������Ҫ��С���ڴ��С�����Ҫ����ҳ
		size_t page_num = batchNum * byte_size >> PAGE_SHIFT;
		
		//���ٸ�һҳ
		if (page_num == 0) {
			page_num = 1;
		}

		return page_num;
	}
};

//64λƽ̨�£�size_t�洢ҳ�����������Ϊ64λƽ̨����2^64 / 2^13(8kΪһҳ) = 2^51ҳ����ô��ҳint�治��
#ifdef _WIN64   //��Ϊ32λƽ̨��û��_WIN64,64λƽ̨�� _WIN32��_WIN64�������ˣ�ֻ�����ж�_WIN64��������
	typedef unsigned long long PAGE_ID;
#elif _WIN32 
	typedef size_t PAGE_ID;
#elif __x86_64__
	//linux 64λ
	typedef unsigned long long PAGE_ID;
#elif __i386__
	//linux 32λ
	typedef size_t PAGE_ID;
#endif

//����������ҳ���ڴ��Ƚṹ
//��Ҫ���Ϊ˫������ڵ㣬��Ϊ����ά�����ڴ�ȫ��û����ʹ�õ�ʱ��Ҫ�����ϲ㣬���м��ͷ���˫�����������
struct Span {
	//û�й��캯���Լ����г�ʼ��
	PAGE_ID _pageId = 0;//����ڴ���ʼҳ��ҳ��
	size_t _n = 0;//��ǰspan��ά����ҳ���� 

	//˫������ṹ
	Span* _next = nullptr;
	Span* _prev = nullptr;

	size_t _useCount = 0;//ʹ�ü��� ==0˵�������ڴ涼�������ˣ�û��thread cache��ʹ��

	void* _freeList = nullptr;//�кõ�С���ڴ���������
};

//��ͷ˫��ѭ������
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

	//ͷ��
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

	//ͷɾ
	Span* PopFront() {
		Span* front = _head->_next;
		//Erase�ﲢû���ͷſռ䣬ֻ���޳���
		Erase(front);
		return front;
	}

	void Erase(Span* pos) {
		assert(pos);
		//���ܰ�ͷ�ڵ�ɾ��
		assert(pos != _head);

		Span* prev = pos->_prev;
		prev->_next = pos->_next;
		pos->_next->_prev = prev;

		//����Ҫdelete����ڵ㣬����Ҫ�����ϲ�
	}

	bool Empty() {
		return _head->_next == _head;
	}

private:
	Span* _head = nullptr;

public:
	std::mutex _mtx;//Ͱ�������Ļ�������̰߳�ȫ����
};