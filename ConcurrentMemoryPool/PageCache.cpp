#include "PageCache.h"

PageCache PageCache::_instance;

Span* PageCache::NewSpan(size_t pageNum) {
	assert(pageNum > 0 && pageNum < PAGE_LIST_SIZE);

	//��Ϊ������һ���ݹ���ã�����ԭ�ȵ�����Դ���������ڻ�û�е���Ӧ��ʹ�õݹ���
	std::lock_guard<std::recursive_mutex> lock(PageCache::GetInstance()->_mtx);

	//�����ǰͰ���п��õĴ���ڴ�
	if (!_spanLists[pageNum].Empty()) {
		//���ز���������ɾ��
		return _spanLists[pageNum].PopFront();
	}

	//�������Ͱ����û�д���ڴ棬����н����з�
	//pageNum+1���ӵ�ǰ����һ����ʼ���
	for (size_t i = pageNum + 1; i < PAGE_LIST_SIZE; ++i) {
		//�ҵ����з�
		if (!_spanLists[i].Empty()) {
			//�õ�һ������ڴ�span
			Span* n_span = _spanLists[i].PopFront();
			
			//��n_span��ͷ���г�һ��pageNumҳ��span
			Span* pageNum_span = new Span;
			//pageNum_span����ʼҳ=n_span����ʼҳ��ҳ��������Ҫ��ҳ��page_num;
			pageNum_span->_pageId = n_span->_pageId;
			pageNum_span->_n = pageNum;

			//����n_span��ҳ������ʼҳ
			n_span->_pageId += pageNum;//�������pageNumҳ
			n_span->_n -= pageNum;//ҳ��������pageNumҳ

			//��ʣ�µĲ����Ӧ������
			_spanLists[n_span->_n].PushFront(n_span);

			return pageNum_span;
		}

	}

	//������˵��pageNum����û�д���ڴ���
	//��ʱ����ڶ�������һ��128ҳ�Ĵ���ڴ�
	Span* bigPage = new Span;
	void* ptr = SystemAlloc((PAGE_LIST_SIZE - 1) << PAGE_SHIFT);

	//������ʼҳ��ҳ��
	bigPage->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigPage->_n = PAGE_LIST_SIZE - 1;

	//���뵽�����Ӧ������
	_spanLists[bigPage->_n].PushFront(bigPage);

	//�ݹ���ã�������дһ���з�
	return NewSpan(pageNum);//ע���������Ҫ�ӵݹ�������ֹ����������
}