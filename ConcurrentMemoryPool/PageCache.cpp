#include "PageCache.h"

PageCache PageCache::_instance;

Span* PageCache::NewSpan(size_t pageNum) {
	assert(pageNum > 0);

	//����128ҳ��ʹ��ϵͳ����
	if (pageNum > PAGE_LIST_SIZE - 1) {
		void* ptr = SystemAlloc(pageNum << PAGE_SHIFT);
		
		//ʹ�ö����ڴ��new
		//Span* span = new Span;
		Span* span = _spanPool.New();
		//ά����ʼҳ��ҳ��
		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = pageNum;

		//����map
		_idSpanMap[span->_pageId] = span;

		return span;
	}

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
			//Span* pageNum_span = new Span;
			Span* pageNum_span = _spanPool.New();

			//pageNum_span����ʼҳ=n_span����ʼҳ��ҳ��������Ҫ��ҳ��page_num;
			pageNum_span->_pageId = n_span->_pageId;
			pageNum_span->_n = pageNum;

			//����n_span��ҳ������ʼҳ
			n_span->_pageId += pageNum;//�������pageNumҳ
			n_span->_n -= pageNum;//ҳ��������pageNumҳ

			//��ʣ�µĲ����Ӧ������
			_spanLists[n_span->_n].PushFront(n_span);

			//��n_span����βҳӳ���Լ�������PageCahe����ʱ�ϲ����в���
			_idSpanMap[n_span->_pageId] = n_span;
			//ע������Ҫ-1 ��Ϊ��ʼҳ����1ҳ
			_idSpanMap[n_span->_pageId + n_span->_n - 1] = n_span;

			//pageNum_span����ά����ÿһҳ��ӳ���Լ� ����CentralCache���ղ���span
			for (PAGE_ID i = 0; i < pageNum_span->_n; ++i) {
				_idSpanMap[pageNum_span->_pageId + i] = pageNum_span;
			}

			return pageNum_span;
		}

	}

	//������˵��pageNum����û�д���ڴ���
	//��ʱ����ڶ�������һ��128ҳ�Ĵ���ڴ�
	//Span* bigPage = new Span;
	Span* bigPage = _spanPool.New();

	void* ptr = SystemAlloc((PAGE_LIST_SIZE - 1) << PAGE_SHIFT);

	//������ʼҳ��ҳ��
	bigPage->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigPage->_n = PAGE_LIST_SIZE - 1;

	//���뵽�����Ӧ������
	_spanLists[bigPage->_n].PushFront(bigPage);

	//�ݹ���ã�������дһ���з�
	return NewSpan(pageNum);//ע���������Ҫ�ӵݹ�������ֹ����������
}

//��ȡҳ�Ŷ�Ӧ��Span
Span* PageCache::MapObjectToSpan(void* ptr) {
	auto it = _idSpanMap.find((PAGE_ID)ptr >> PAGE_SHIFT);
	if (it != _idSpanMap.end()) {
		return it->second;
	}
	else {
		assert(false);
		return nullptr;
	}
}

//�ͷſ��е�Span��PageCache,���ϲ����ڵ�span
void PageCache::ReleaseSpanToPageCache(Span* span) {
	//��spanǰ���ҳ�����Խ��кϲ��������ڴ���Ƭ������
	
	//����Ǵ���128ҳ�ģ�����ϵͳ����ģ�ֱ�ӻ�����
	if (span->_n > PAGE_LIST_SIZE - 1) {
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);

		//ʹ�ö����ڴ���ͷ�
		//delete span;
		_spanPool.Delete(span);
		return;
	}
	
	//��ǰ�ϲ�
	while (true) {
		//���Ժϲ���ǰһ��span����ʼҳ��
		PAGE_ID prevId = span->_pageId - 1;
		auto it = _idSpanMap.find(prevId);
		
		//ǰ���ҳ��û��span���˳�
		if (it == _idSpanMap.end()) {
			break;
		}

		//ǰ���ҳ��span����ʹ�ã��˳�
		Span* prev = it->second;
		if (prev->_isUse == true) {
			break;
		}
		//�ϲ���ҳ������PageCache��������ҳ��Ҳû�취�ϲ����˳�
		if (prev->_n + span->_n > PAGE_LIST_SIZE - 1) {
			break;
		}

		//������˵�����Ժϲ�
		span->_pageId = prev->_pageId;
		span->_n += prev->_n;

		//ǰһ�����ϲ�������������Ƴ�
		_spanLists[prev->_n].Erase(prev);
		//delete prev;
		_spanPool.Delete(prev);
	}

	//���ϲ�
	while (true) {
		//���Ժϲ��ĺ�һ��span����ʼҳ��
		PAGE_ID nextId = span->_pageId + span->_n;
		auto it = _idSpanMap.find(nextId);
		if (it == _idSpanMap.end()) {
			break;
		}

		Span* next = it->second;
		if (next->_isUse) {
			break;
		}

		if (span->_n + next->_n > PAGE_LIST_SIZE - 1) {
			break;
		}

		//���ϲ���ʼҳ���ñ�
		span->_n += next->_n;

		_spanLists[next->_n].Erase(next);
		//delete next;
		_spanPool.Delete(next);
	}

	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;
	
	//��βҳ�Ŷ�Ҫӳ��
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId + span->_n - 1] = span;
}