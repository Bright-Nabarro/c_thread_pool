#include "ctp/ts_queue.h"
#include "ctp_util.h"

#include <stdlib.h>

typedef struct ctp_node
{
	void* data;
	struct ctp_node* next;
} ctp_node;

typedef struct ctp_queue
{
	ctp_node* head;
	ctp_node* tail;
	pthread_mutex_t head_mtx;
	pthread_mutex_t tail_mtx;
	pthread_cond_t cond;
	atomic_bool wait_terminate;
} ctp_queue_t;


ctp_queue_t* ctp_queue_create(int* pec)
{
	int ec = 0;
	INI_EC(pec);
	ctp_queue_t* pque = malloc(sizeof(ctp_queue_t));
	ERR_PTR_ERRNO(pque, pec, return NULL);

	pque->head = calloc(1, sizeof(ctp_node));
	ERR_PTR_ERRNO(pque->head, pec, return NULL);
	pque->tail = pque->head;

	ec = pthread_mutex_init(&pque->head_mtx, NULL);
	ERR_INT(ec, pec, return NULL);
	
	ec = pthread_mutex_init(&pque->tail_mtx, NULL);
	ERR_INT(ec, pec, return NULL);

	ec = pthread_cond_init(&pque->cond, NULL);
	ERR_INT(ec, pec, return NULL);

	pque->wait_terminate = false;

	return pque;
}

void** ctp_queue_destroy(ctp_queue_t* pque, size_t* node_size, int* pec)
{
	INI_EC(pec);
	int ec;

	size_t capacity = 10;
	size_t cnt = 0;
	void** ret_array = (void**)malloc(capacity * sizeof(void*));
	ERR_PTR_ERRNO(ret_array, pec, return NULL);

	while(!ctp_queue_empty(pque, pec))
	{
		ERR_PEC(pec, return NULL);

		if (cnt == capacity)
		{
			capacity *= 2;
			ret_array = realloc(ret_array, capacity * sizeof(void*));
			ERR_PTR_ERRNO(ret_array, pec, return NULL);
		}

		ret_array[cnt++] = ctp_queue_wait_pop(pque, pec);
		ERR_PEC(pec, return NULL);
	}

	if (cnt == 0)
	{
		free(ret_array);
		ret_array = NULL;
	}
	else
	{
		ret_array = realloc(ret_array, cnt * sizeof(void*));
		ERR_PTR_ERRNO(ret_array, pec, return NULL);
	}
	
	if (node_size != NULL)
	{
		*node_size = cnt;
	}

	ec = pthread_cond_destroy(&pque->cond);
	ERR_INT(ec, pec, return NULL);
	ec = pthread_mutex_destroy(&pque->tail_mtx);
	ERR_INT(ec, pec, return NULL);
	ec = pthread_mutex_destroy(&pque->head_mtx);
	ERR_INT(ec, pec, return NULL);

	free(pque->tail);
	free(pque);
	return ret_array;
}

void ctp_queue_push(ctp_queue_t* pque, void* value, int* pec)
{
	INI_EC(pec);
	int ec;
	// new_tail 是新的哨兵节点，排在队尾
	ctp_node* new_tail = calloc(1, sizeof(ctp_node));
	ERR_PTR_ERRNO(new_tail, pec, return);

	ec = pthread_mutex_lock(&pque->tail_mtx);
	ERR_INT(ec, pec, return);

	pque->tail->data = value;
	pque->tail->next = new_tail;
	pque->tail = new_tail;
	
	ec = pthread_mutex_unlock(&pque->tail_mtx);
	ERR_INT(ec, pec, return);

	// 短暂持头锁
	ec = pthread_mutex_lock(&pque->head_mtx);
	ERR_INT(ec, pec, return);
	ec = pthread_cond_signal(&pque->cond);
	ERR_INT(ec, pec, {
		pthread_mutex_unlock(&pque->head_mtx);
		return;
	});
	ec = pthread_mutex_unlock(&pque->head_mtx);
	ERR_INT(ec, pec, return);
}

static ctp_node* queue_get_tail(ctp_queue_t* pque, int* pec)
{
	int ec;
	ec = pthread_mutex_lock(&pque->tail_mtx);
	ERR_INT(ec, pec, return NULL);
	ctp_node* cur_tail = pque->tail;
	ec = pthread_mutex_unlock(&pque->tail_mtx);
	ERR_INT(ec, pec, return NULL);
	
	return cur_tail;
}

void* ctp_queue_try_pop(ctp_queue_t* pque, int* pec)
{
	INI_EC(pec);
	int ec;
	ec = pthread_mutex_lock(&pque->head_mtx);
	THDPT("lock head");
	ERR_INT(ec, pec, return NULL);

	ctp_node* cur_tail = queue_get_tail(pque, pec);
	ERR_INT(ec, pec, {
		pthread_mutex_unlock(&pque->head_mtx);
		THDPT("unlock head");
		return NULL;
	});

	if (pque->head == cur_tail)
	{
		ec = pthread_mutex_unlock(&pque->head_mtx);
		THDPT("unlock head");
		ERR_INT(ec, pec, return NULL);
		return NULL;
	}
	ctp_node* old_head = pque->head;
	pque->head = pque->head->next;

	ec = pthread_mutex_unlock(&pque->head_mtx);
	THDPT("unlock head");
	ERR_INT(ec, pec, return NULL);
	
	void* ret = old_head->data;
	free(old_head);
	return ret;
}

void* ctp_queue_wait_pop(ctp_queue_t* pque, int* pec)
{
	INI_EC(pec);
	int ec;
	ec = pthread_mutex_lock(&pque->head_mtx);
	THDPT("lock head");
	ERR_INT(ec, pec, return NULL);

	if (atomic_load(&pque->wait_terminate))
	{
		ec = pthread_mutex_unlock(&pque->head_mtx);
		THDPT("unlock head");
		ERR_INT(ec, pec, return NULL);
		return NULL;
	}

	while (pque->head == queue_get_tail(pque, pec))
	{
		ERR_PEC(pec, return NULL);
		THDPT("cond wait, unlock head");
		ec = pthread_cond_wait(&pque->cond, &pque->head_mtx);
		THDPT("cond triggered, lock head");
		ERR_INT(ec, pec, {
			pthread_mutex_unlock(&pque->head_mtx);
			return NULL;
		});

		if (atomic_load(&pque->wait_terminate))
		{
			ec = pthread_mutex_unlock(&pque->head_mtx);
			THDPT("unlock head");
			ERR_INT(ec, pec, return NULL);
			return NULL;
		}
	}
	
	ctp_node* old_head = pque->head;
	pque->head = pque->head->next;

	ec = pthread_mutex_unlock(&pque->head_mtx);
	THDPT("unlock head");
	ERR_INT(ec, pec, return NULL);

	void* ret = old_head->data;
	free(old_head);
	return ret;
}

bool ctp_queue_empty(ctp_queue_t* pque, int* pec)
{
	int ec;
	INI_EC(pec);
	ec = pthread_mutex_lock(&pque->head_mtx);
	THDPT("lock head");
	ERR_INT(ec, pec, return false);
	
	ctp_node* tail = queue_get_tail(pque, pec);
	ERR_INT(ec, pec, {
		pthread_mutex_unlock(&pque->head_mtx);
		return false;
	});
	bool ret = pque->head == tail;

	ec = pthread_mutex_unlock(&pque->head_mtx);
	THDPT("unlock head");
	ERR_INT(ec, pec, return false);

	return ret;
}

void ctp_queue_all_wait_terminate(ctp_queue_t* pque, int* pec)
{
	int ec;
	INI_EC(pec);
	ec = pthread_mutex_lock(&pque->head_mtx);
	THDPT("lock head");
	ERR_INT(ec, pec, return);

	atomic_store(&pque->wait_terminate, true);

	ec = pthread_cond_broadcast(&pque->cond);
	THDPT("cond boardcast");
	ERR_INT(ec, pec, {
		pthread_mutex_unlock(&pque->head_mtx);
		return;
	});

	ec = pthread_mutex_unlock(&pque->head_mtx);
	THDPT("unlock head");
	ERR_INT(ec, pec, return);
}

void ctp_queue_wait_reset(ctp_queue_t* pque)
{
	atomic_store(&pque->wait_terminate, true);
}
