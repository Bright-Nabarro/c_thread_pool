#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include "thd_pool.h"
#ifdef THD_POOL_DEBUG
#include <stdio.h>
#endif

#if !THD_POOL_SUPPORT_C23
#    define nullptr NULL
#endif

#ifdef THD_POOL_DEBUG
#define DEBUG_PRINT(msg, ...)                                                  \
	do                                                                         \
	{                                                                          \
		fprintf(stderr, msg "\n" __VA_OPT__(, ) __VA_ARGS__);                  \
	} while (0)
#define DEBUG_PRINT_EC(msg, ec, ...)                                           \
	do                                                                         \
	{                                                                          \
		thd_pool_perror(msg, ec __VA_OPT__(, ) __VA_ARGS__);                   \
	} while(0)
#else
#define DEBUG_PRINT(msg, ...)
#define DEBUG_PRINT_EC(msg, ec, ...)
#endif

#define OPT (void)

typedef struct _job
{
    void (*j_func)(void*);
    void* j_param;
    struct _job* j_next;
} job;

typedef struct _queue
{
    job* q_sentinel;
    job* q_tail;
    pthread_mutex_t q_mtx;
    pthread_cond_t q_cond;
} queue;

typedef struct _thd_pool
{
    pthread_t* p_thds;
    size_t p_thds_size;
	pthread_barrier_t p_start;
    queue* p_que;
	// 整个声明周期变化为 [unkown] -> true -> false, 达到最后的false后不可更改状态
    atomic_bool p_working;
} thd_pool;

struct thd_args
{
	
};


/*
 *    STATIC DECLARE
 */
static thd_pool_error_code err_int2err_enum(int);
#define ERR_INT(int_ec, p_enum_ec, next_process)                               \
	do                                                                         \
	{                                                                          \
		if (p_enum_ec == nullptr)                                              \
			break;                                                             \
                                                                               \
		if (int_ec)                                                            \
		{                                                                      \
			DEBUG_PRINT("error happend in file: %s, line:%d, func: %s",        \
						__FILE__, __LINE__, __func__);                         \
			DEBUG_PRINT_EC(__func__, int_ec);                                  \
                                                                               \
			(*p_enum_ec) = err_int2err_enum(int_ec);                           \
			next_process;                                                      \
		}                                                                      \
	} while (0)

#define ERR_PTR(ptr, p_enum_ec, next_process)                                  \
	do                                                                         \
	{                                                                          \
		if ((p_enum_ec) == nullptr)                                            \
			break;                                                             \
		if ((ptr) == nullptr)                                                  \
		{                                                                      \
			DEBUG_PRINT("error happend in file: %s, line:%d, func: %s",        \
						__FILE__, __LINE__, __func__);                         \
			DEBUG_PRINT_EC(__func__, errno);                                   \
                                                                               \
			(*p_enum_ec) = err_int2err_enum(errno);                            \
			next_process;                                                      \
		}                                                                      \
	} while (0)

static_assert(sizeof(void*) >= 4, "thd_fn return void* as int type");

static void job_destroy(job* pjob);
static inline bool queue_empty(queue* pque);
static inline job* queue_deque(queue* pque);
static inline void queue_inque(queue* pque, job* pjob);
static queue* queue_create(thd_pool_error_code* ec);
static void queue_destroy(queue* pque, thd_pool_error_code* ec);
static void* thd_fn(void*);


/*
 *    INTERFACE IMPLEMENT
 */

const char* thd_pool_error_str(thd_pool_error_code error_code)
{
	return strerror((int)error_code);
}

void thd_pool_perror(const char* msg, thd_pool_error_code error_code)
{
	fprintf(stderr, "%s %s\n", msg, thd_pool_error_str(error_code));
}

struct _job* thd_pool_job_create(void (*func)(void*), void* param,
                                 thd_pool_error_code* ec)
{
	if (ec) *ec = thd_pool_success;
    job* pjob = malloc(sizeof(job));
    if (pjob == nullptr)
    {
        *ec = err_int2err_enum(errno);
        return nullptr;
    }

    pjob->j_func = func;
    pjob->j_param = param;
    pjob->j_next = nullptr;
    return pjob;
}

void thd_pool_emplace_job(thd_pool* ppl, void (*func)(void*), void* param,
						  thd_pool_error_code* ec)
{
	job* pjob = thd_pool_job_create(func, param, ec);
	ERR_PTR(pjob, ec, return);

	thd_pool_add_job(ppl, pjob, ec);
}

thd_pool* thd_pool_create(size_t thd_size, thd_pool_error_code* ec)
{
	if (ec) *ec = thd_pool_success;
	int err;
	thd_pool* ppool = malloc(sizeof(thd_pool));
	ERR_PTR(ppool, ec, return nullptr);
	ppool->p_thds = malloc(thd_size * sizeof(pthread_t));
	ERR_PTR(ppool->p_thds, ec, return nullptr);

	ppool->p_thds_size = thd_size;
	assert(thd_size + 1 > 0);
	err = pthread_barrier_init(&ppool->p_start, nullptr, thd_size+1);
	ERR_INT(err, ec, return nullptr);
	DEBUG_PRINT("main thread: init barrier %zu", thd_size+1);

	ppool->p_que = queue_create(ec);
	ERR_PTR(ppool->p_que, ec, return nullptr);

	for (size_t i = 0; i < thd_size; ++i)
	{
		err = pthread_create(&ppool->p_thds[i], NULL, thd_fn, ppool);
		DEBUG_PRINT("main thread: pthread %lx created", ppool->p_thds[i]);
		ERR_INT(err, ec, return nullptr);
	}
	DEBUG_PRINT("main thread pool create %zu thread success", thd_size);

	ppool->p_working = true;
	DEBUG_PRINT("main thread: before barrier wait");
	err = pthread_barrier_wait(&ppool->p_start);
	if (err != 0 && err != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		ERR_INT(err, ec, return nullptr);
	}
	DEBUG_PRINT("main thread: after barrier wait");

	return ppool;
}

int thd_pool_destroy(thd_pool* pool, thd_pool_error_code** ecs)
{
	int err = 0, result = 0;
	atomic_store(&pool->p_working, false);
	err = pthread_mutex_lock(&pool->p_que->q_mtx);
	ERR_INT(err, &result, return result);
	err = pthread_cond_broadcast(&pool->p_que->q_cond);
	ERR_INT(err, &result, return result);
	err = pthread_mutex_unlock(&pool->p_que->q_mtx);
	ERR_INT(err, &result, return result);
	DEBUG_PRINT("main thread: pthread cond broadcast");
	if (ecs != nullptr)
	{
		ecs = calloc(pool->p_thds_size, sizeof(int*));
		ERR_PTR(ecs, &result, return result);
	}
	
	for (size_t i = 0; i < pool->p_thds_size; ++i)
	{
		void* ret;
		int ec;
		err = pthread_join(pool->p_thds[i], &ret);
		ERR_INT(err, &ec, result = -1);
		ERR_INT((int)(long)ret, &ec, result = -1);
		if (ecs != nullptr)
			ecs[i] = &ec;
		DEBUG_PRINT("main thread: pthread %lx joined", pthread_self());
	}

	DEBUG_PRINT("main thread: all thread joined");

	assert(queue_empty(pool->p_que));
	queue_destroy(pool->p_que, &result);
	err = pthread_barrier_destroy(&pool->p_start);
	ERR_INT(err, &result, return result);
	free(pool);
	return result;
}

void thd_pool_add_job(thd_pool* ppl, struct _job* job, thd_pool_error_code* ec)
{
	if (ec) *ec = thd_pool_success;
	int err;
	queue* pque = ppl->p_que;

	err = pthread_mutex_lock(&pque->q_mtx);
	ERR_INT(err, ec, return);

	DEBUG_PRINT("main thread prepare a job to inque");
	queue_inque(pque, job);
	err = pthread_cond_signal(&pque->q_cond);
	ERR_INT(err, ec, return);
	DEBUG_PRINT("main thread a job inque success");

	err = pthread_mutex_unlock(&pque->q_mtx);
	ERR_INT(err, ec, return);
}

/*
 * ISTATIC MPLEMENT
 */

static void job_destroy(job* pjob)
{
	free(pjob);
}

static thd_pool_error_code err_int2err_enum(int ec)
{
    return (thd_pool_error_code)ec;
}

// need lock queue outside
static inline bool queue_empty(queue* pque)
{
	return pque->q_sentinel->j_next == nullptr;
}

static inline job* queue_deque(queue* pque)
{
	assert(!queue_empty(pque));
	job* pjob = pque->q_sentinel->j_next;
	pque->q_sentinel->j_next = pque->q_sentinel->j_next->j_next;
	return pjob;
}

static inline void queue_inque(queue* pque, job* pjob)
{
	pque->q_tail->j_next = pjob;
	pque->q_tail = pque->q_tail->j_next;
}

static queue* queue_create(thd_pool_error_code* ec)
{
    queue* pque = calloc(1, sizeof(queue));
	ERR_PTR(pque, ec, return nullptr);

    job* sentinel = thd_pool_job_create(nullptr, nullptr, ec);
	ERR_PTR(sentinel, ec, return nullptr);
	pque->q_sentinel = sentinel;

	pque->q_tail = (job*)pque->q_sentinel;

    int err;
    err = pthread_mutex_init(&pque->q_mtx, nullptr);
	ERR_INT(err, ec, return nullptr);

    err = pthread_cond_init(&pque->q_cond, nullptr);
	ERR_INT(err, ec, return nullptr);

    return pque;
}

static void queue_destroy(queue* pque, thd_pool_error_code* ec)
{
	int err;
#ifdef THD_POOL_DEBUG
	err = pthread_mutex_lock(&pque->q_mtx);
	ERR_INT(err, ec, return);

	assert(queue_empty(pque));

	err = pthread_mutex_unlock(&pque->q_mtx);
	ERR_INT(err, ec, return);
#endif

	err = pthread_mutex_destroy(&pque->q_mtx);
	ERR_INT(err, ec, return);

	err = pthread_cond_destroy(&pque->q_cond);
	ERR_INT(err, ec, return);

	free(pque);
}

static void* thd_fn(void* arg)
{
	thd_pool* ppl = (thd_pool*) arg;
	queue* pque = ppl->p_que;
	thd_pool_error_code result = 0;
	int err;
	DEBUG_PRINT("pthread %lx before barrier wait", pthread_self());
	err = pthread_barrier_wait(&ppl->p_start);
	if (err != 0 && err != PTHREAD_BARRIER_SERIAL_THREAD)
	{
		ERR_INT(err, &result, return (void*)(long)result);
	}
	DEBUG_PRINT("pthread %lx after barrier wait", pthread_self());

	while(true)
	{
		err = pthread_mutex_lock(&pque->q_mtx);	
		ERR_INT(err, &result, return (void*)(long)result);
		// 只有当队列消耗完毕并且working flag为false时线程走向完结
		if (queue_empty(pque) && !atomic_load(&ppl->p_working))
		{
			err = pthread_mutex_unlock(&pque->q_mtx);
			ERR_INT(err, &result, return (void*)(long)result);
			break;
		}
		
		while(queue_empty(pque))
		{
			err = pthread_cond_wait(&pque->q_cond, &pque->q_mtx);
			ERR_INT(err, &result, return (void*)(long)result);
			if (queue_empty(pque))
			{
				if (!atomic_load(&ppl->p_working))
				{
					err = pthread_mutex_unlock(&pque->q_mtx);
					ERR_INT(err, &result, );
					DEBUG_PRINT("pthread %lx end in mid", pthread_self());
					return (void*)(long)result;
				}
				else
				{
					break;
				}
			}
		}
		
		job* value = queue_deque(pque);
		DEBUG_PRINT("pthread %lx take out job from queue", pthread_self());

		err = pthread_mutex_unlock(&pque->q_mtx);
		ERR_INT(err, &result, return (void*)(long)result);

		value->j_func(value->j_param);
		job_destroy(value);
		DEBUG_PRINT("pthread %lx consume job", pthread_self());
	}
	
		DEBUG_PRINT("pthread %lx end in last", pthread_self());
	return (void*)(long)result;
}

