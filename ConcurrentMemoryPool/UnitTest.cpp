#include "objectPool.h"
#include "ConcurrentAlloc.h"
#include <vector>
//测试TLS
void Alloc1() {
	for (size_t i = 0; i < 5; ++i) {
		void* ptr = ConcurrentAlloc(6);
	}
}
void Alloc2() {
	for (size_t i = 0; i < 5; ++i) {
		void* ptr = ConcurrentAlloc(7);
	}
}
void TLSTest() {
	std::thread t1(Alloc1);
	std::thread t2(Alloc2);

	t1.join();
	t2.join();
}

void TestConcurrentAlloc1() {
	void* ptr1 =ConcurrentAlloc(6);
	void* ptr2 =ConcurrentAlloc(7);
	void* ptr3 =ConcurrentAlloc(1);
}

void BigAlloc() {
	void* ptr1 = ConcurrentAlloc(257 * 1024);//大于256k，但pageCahe的页数还可以申请
	ConcurrentFree(ptr1, 257 * 1024);

	void* ptr2 = ConcurrentAlloc(129 * 8 * 1024);//pageCache的页数页无法申请
	ConcurrentFree(ptr2, 129 << PAGE_SHIFT);
}

int main() {

	//TestObjectPool();
	//TLSTest();

	TestConcurrentAlloc1();
	
	return 0;
}