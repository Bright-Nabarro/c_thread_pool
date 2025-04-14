#include "ctp/thd_pool.h"

#include "ctp/ts_queue.h"
#include "ctp_util.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

typedef struct ctp_thdpool
{
	ctp_queue* que;
	pthread_barrier_t start_bar;
	atomic_bool waiting;
	atomic_bool shutdown;
	pthread_t* tids;
	size_t thd_size;
}  ctp_thdpool;

typedef struct ctp_job
{
	void(*func)(void*);
	void* args;
	void(*cleanup)(void*);
} ctp_job;

typedef struct thd_arg
{
	ctp_thdpool* tp;
} thd_arg;

static void do_job(ctp_job* job)
{
	job->func(job->args);
	if (job->cleanup)
		job->cleanup(job->args);
	free(job);

}

static
void* thread_fn(void* vargs)
{
	int* ec = calloc(1, sizeof(int));
	thd_arg* arg = vargs;
	ctp_thdpool* tp = arg->tp;

	*ec = pthread_barrier_wait(&tp->start_bar);
	if (*ec != 0 && *ec != PTHREAD_BARRIER_SERIAL_THREAD)
		return ec;

	while (atomic_load(&tp->shutdown) || !ctp_queue_empty(tp->que, ec))
	{
		ERR_PEC(ec, return ec);

		ctp_job* job = ctp_queue_wait_pop(tp->que, ec);
		ERR_PEC(ec, return ec);
		if (job == NULL)
		{
			if (atomic_load(&tp->shutdown))
				break;
			continue;
		}
		
		do_job(job);
	}

	free(vargs);
	return ec;
}

static
ctp_job* ctp_job_create(void (*fn)(void*), void* args,
							   void (*cleanup)(void*), int* ec)
{
	ctp_job* job = malloc(sizeof(ctp_job));
	ERR_PTR_ERRNO(job, ec, return NULL);
	job->func = fn;
	job->args = args;
	job->cleanup = cleanup;

	return job;
}

/****
 * INTERFACE IMPLAMENTATION
 ****/
ctp_thdpool* ctp_thdpool_create(size_t thd_size, int* pec)
{
	INI_EC(pec);
	ctp_thdpool* thd_pool = malloc(sizeof(ctp_thdpool));
	ERR_PTR_ERRNO(thd_pool, pec, return NULL);
	
	thd_pool->que = ctp_queue_create(pec);
	ERR_PEC(pec, return NULL);

	int ec;
	ec = pthread_barrier_init(&thd_pool->start_bar, NULL, thd_size+1);
	if (ec != 0 && ec != PTHREAD_BARRIER_SERIAL_THREAD)
		return NULL;

	thd_pool->waiting = false;
	thd_pool->shutdown = false;
	thd_pool->tids = malloc(sizeof(pthread_t) * thd_size);
	thd_pool->thd_size = thd_size;
	ERR_PTR_ERRNO(thd_pool->tids, pec, return NULL);

	for (size_t i = 0; i < thd_size; ++i)
	{
		thd_arg* arg = malloc(sizeof(thd_arg));
		ERR_PTR_ERRNO(arg, pec, return NULL);
		arg->tp = thd_pool;

		ec = pthread_create(&thd_pool->tids[i], NULL, thread_fn, arg);
		ERR_INT(ec, pec, return NULL);
	}
	
	ec = pthread_barrier_wait(&thd_pool->start_bar);
	if (ec != 0 && ec != PTHREAD_BARRIER_SERIAL_THREAD)
		ERR_INT(ec, pec, return NULL);
	
	return thd_pool;
}

void ctp_thdpool_add_job(ctp_thdpool* thp, void (*fn)(void*), void* args,
						 void (*cleanup)(void*), int* pec)
{
	INI_EC(pec);
	ctp_job* job = ctp_job_create(fn, args, cleanup, pec);
	ERR_PEC(pec, return;);

	ctp_queue_push(thp->que, job, pec);
	ERR_PEC(pec, return;);
}

bool ctp_thdpool_empty(ctp_thdpool* thp, int* ec)
{
	return ctp_queue_empty(thp->que, ec);
}

void ctp_thdpool_wait(ctp_thdpool* thp, int* ec)
{
	(void)thp, (void)ec;
}

void ctp_thdpool_destroy(ctp_thdpool* thp, int* pec, int** errlist,
						 size_t* errlist_size)
{
	INI_EC(pec);
	atomic_store(&thp->shutdown, true);

	if (errlist_size)
		*errlist_size = thp->thd_size;
	
	int ec;
	bool thd_err_flag = false;
	int* thdec;
	int* thd_errlist = calloc(thp->thd_size, sizeof(int));
	ERR_PTR_ERRNO(thd_errlist, pec, {
		free(thd_errlist);
		return;
	});

	ctp_queue_all_wait_terminate(thp->que, pec);
	ERR_PEC(pec, return);

	for (size_t i = 0; i < thp->thd_size; ++i)
	{
		ec = pthread_join(thp->tids[i], (void**)&thdec);
		ERR_INT(ec, pec, return);
		thd_errlist[i] = *thd_errlist;
		if (thd_errlist[i])
			thd_err_flag = true;
		free(thdec);
	}
	free(thp->tids);

	while(!ctp_queue_empty(thp->que, pec))
	{
		ERR_PEC(pec, return);
		ctp_job* job = ctp_queue_try_pop(thp->que, pec);
		ERR_PEC(pec, return);
		assert(job != NULL);
		do_job(job);
	}

	if (!errlist || !errlist)
	{
		if (errlist_size)
			*errlist_size = 0;
		free(thd_errlist);
	}
	else if (!thd_err_flag)
	{
		free(thd_errlist);
		*errlist = NULL;
		*errlist_size = 0;

	}
	else
	{
		*errlist = thd_errlist;
	}

	size_t left_node_size;
	assert(ctp_queue_empty(thp->que, NULL));
	void** queret = ctp_queue_destroy(thp->que, &left_node_size, pec);
	assert(left_node_size == 0);
	assert(queret == NULL);

	ec = pthread_barrier_destroy(&thp->start_bar);
	free(thp);
	ERR_INT(ec, pec, return);
}

