#pragma once
#include "Common.h"

//�����ڴ��

//template <size_t N>//������ģ�����
//class ObjectPool {
//
//};

template <class T>
class ObjectPool {
public:
	T* New() {
		T* obj = nullptr;

		//�����������п����ڴ��ʱʹ�����������ڴ��
		if (_freeList) {
			//ͷɾ
			void* next = *(void**)_freeList;
			obj = (T*)_freeList;
			_freeList = next;
		}
		//����ʹ���ڴ�ط����ڴ�
		else {
			size_t objSize = sizeof(T);

			//��ʣ���ڴ�С��T���ʹ�Сʱ����������һ��������ô�����������ڴ�
			if (_reminSize < objSize) {
				_reminSize = 128 * 1024;
				
				//_memory = (char*)malloc(_reminSize);
				//ֱ�Ӳ����ѣ���ʹ��malloc����
				_memory = (char*)SystemAlloc(_reminSize);
				if (_memory == nullptr) {
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory;
			//��T����С��һ��ָ��Ĵ�Сʱ��Ϊ��������������洢ָ������󣬷���ָ��Ĵ�С
			objSize = objSize < sizeof(void*) ? sizeof(void*) : sizeof(T);
			
			//�ڴ����ʼλ�ú���
			_memory += objSize;

			//����ʣ���ڴ���С
			_reminSize -= objSize;
		}

		//��λnew ������ʼ�����пռ䣬�����Զ������͵Ĺ��캯��
		new(obj)T;

		return obj;
	}

	void Delete(T* obj) {
		//��ʾ������������
		obj->~T();

		//ͷ��
		//obj��ǰ4/8���ֽ������洢��ǰ�ĵ�һ����ʼλ��
		//����д������32λ����64λƽ̨���������Զ�����ָ��Ĵ�С
		*(void**)obj = _freeList;
		//���������������ʼλ��
		_freeList = obj;
	}

private:
	//ȱʡֵ
	char* _memory = nullptr;//����Ĵ���ڴ�
	void* _freeList = nullptr;//�ͷŵ�����
	size_t _reminSize = 0;//ʣ���ڴ���С
};



//���Զ����ڴ������
struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{

	//��֤���������ڴ��Ƿ��ظ����õ�����
	ObjectPool<TreeNode> tnPool;
	TreeNode* node1 = tnPool.New();
	TreeNode* node2 = tnPool.New();
	cout << node1 << endl;
	cout << node2 << endl;

	tnPool.Delete(node1);
	TreeNode* node3 = tnPool.New();
	cout << node3 << endl;

	cout << endl;

	//��֤�ڴ�ص��׿첻�죬��û���������ܵ��Ż�

	const size_t Rounds = 10;//�����ͷŵ��ִ�
	const size_t N = 100000;//ÿ�������ͷŵĴ���

	std::vector<TreeNode*> v1;
	v1.reserve(N);

#ifdef _WIN32
	size_t begin1 = clock();
#else
	struct timeval begin1;
	gettimeofday(&begin1, nullptr);
#endif

	for (size_t i = 0; i < Rounds; ++i) {
		for (size_t i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (size_t i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}

#ifdef _WIN32
	size_t end1 = clock();
#else
	struct timeval end1;
	gettimeofday(&end1, nullptr);
#endif


	//�������ǵ����Լ���д���ڴ��
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	ObjectPool<TreeNode> tnPool1;

#ifdef _WIN32
	size_t begin2 = clock();
#else
	struct timeval begin2;
	gettimeofday(&begin2, nullptr);
#endif
	for (size_t i = 0; i < Rounds; ++i) {
		for (size_t i = 0; i < N; ++i)
		{
			v2.push_back(tnPool1.New());
		}
		for (size_t i = 0; i < N; ++i)
		{
			tnPool1.Delete(v2[i]);
		}

		v2.clear();
	}

#ifdef _WIN32
	size_t end2 = clock();

	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pool cost time:" << end2 - begin2 << endl;
#else
	//timeval�ṹ�嶨��
	/*
	struct  timeval {
		long  tv_sec;  //��
		long  tv_usec; //΢��
	};
	*/

	struct timeval end2;
	gettimeofday(&end2, nullptr);

	//�Ƚ���ת΢�� ��+����1���΢�� = ������΢��������/1000����Ϊ����
	double costTime1 = ((end1.tv_sec - begin1.tv_sec) * 1000000 + (end1.tv_usec - begin1.tv_usec)) / 1000;
	double costTime2 = ((end2.tv_sec - begin2.tv_sec) * 1000000 + (end2.tv_usec - begin2.tv_usec)) / 1000;
	
	cout << "new cost time:" << costTime1 << endl;
	cout << "object pool cost time:" << costTime2 << endl;
#endif	 
}