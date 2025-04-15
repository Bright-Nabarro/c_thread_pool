#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "debug.h"
#include "ctp/ts_queue.h"



#define THD_FN_SCALE 1000
static 
void* thd_producer1(void* arg)
{
	ctp_queue* que = arg;
	int ec;
	for (size_t i = 0; i < THD_FN_SCALE; ++i)
	{
		int* n = malloc(sizeof(int));
		*n = i;
		ctp_queue_push(que, n, &ec);
		assert(ec == 0);
	}
	return NULL;
}

static
void* thd_consume1 (void* arg)
{
	ctp_queue* que = arg;
	int ec;
	for (size_t i = 0; i < THD_FN_SCALE; ++i)
	{
		int* n = ctp_queue_wait_pop(que, &ec);
		assert(ec == 0);
		assert(*n == (int)i);
		free(n);
	}
	return NULL;
}

static
void test_push_wait_pop_base()
{
	ctp_queue* que = ctp_queue_create(NULL);
	pthread_t p[2];
	pthread_create(&p[0], NULL, thd_producer1, que);
	pthread_create(&p[1], NULL, thd_consume1, que);
	DPT("pthread create");
	pthread_join(p[0], NULL);
	DPT("producer join");
	pthread_join(p[1], NULL);
	DPT("consumer join");
	assert(ctp_queue_empty(que, NULL));
	ctp_queue_destroy(que, NULL, NULL);
	DPT("success");
}

// 只能从汇编层面验证原子数组访问是否是原子的，标准没有保证
atomic_int* consum_arr2;

static
void* thd_consume2(void* arg)
{
	ctp_queue* que = arg;
	for (size_t i = 0; i < THD_FN_SCALE; ++i)
	{
		int* n = ctp_queue_wait_pop(que, NULL);
		++consum_arr2[*n];
		free(n);
	}

	return NULL;
}

static
void test_push_wait_pop_more_thds(size_t thds)
{
	consum_arr2 = calloc(THD_FN_SCALE, sizeof(int));

	pthread_t pros[thds];
	pthread_t cons[thds];
	ctp_queue* que = ctp_queue_create(NULL);
	for (size_t i = 0; i < thds; ++i)
	{
		pthread_create(&pros[i], NULL, thd_producer1, que);
		pthread_create(&cons[i], NULL, thd_consume2, que);
	}
	DPT("pthread create");

	for (size_t i = 0; i < thds; ++i)
	{
		pthread_join(pros[i], NULL);
		pthread_join(cons[i], NULL);
	}
	DPT("pthread join");

	for (size_t i = 0; i < THD_FN_SCALE; ++i)
	{
		assert(consum_arr2[i] == thds);
	}
	assert(ctp_queue_empty(que, NULL));
	free(consum_arr2);
	size_t left;
	void** ret = ctp_queue_destroy(que, &left, NULL);
	assert(ret == NULL);
	assert(left == 0);

	DPT("success");
}

struct arg3
{
	ctp_queue* que;
	atomic_bool* flag;
	size_t thds;
};


static
void* thd_consume3(void* arg)
{
	struct arg3* a = arg;
	ctp_queue* que = a->que;
	atomic_bool* flag = a->flag;
	size_t thds = a->thds;
	size_t cnt = 0;
	while(atomic_load(flag) || !ctp_queue_empty(que, NULL))
	{
		int* n = ctp_queue_try_pop(que, NULL);
		if (n != NULL)
		{
			++cnt;
		}
		free(n);
	}
	assert(cnt == thds * THD_FN_SCALE);
	free(arg);

	return NULL;
}


static
void test_push_try_pop(size_t thds)
{
	ctp_queue* que = ctp_queue_create(NULL);
	pthread_t pros[thds];
	pthread_t cons;
	for (size_t i = 0; i < thds; ++i)
	{
		pthread_create(&pros[i], NULL, thd_producer1, que);
	}
	
	atomic_bool flag = true;
	struct arg3* arg = malloc(sizeof(struct arg3));
	arg->flag = &flag;
	arg->thds = thds;
	arg->que = que;

	pthread_create(&cons, NULL, thd_consume3, arg);
	DPT("pthread create");
	
	for (size_t i = 0; i < thds; ++i)
	{
		pthread_join(pros[i], NULL);
	}
	atomic_store(&flag, false);
	pthread_join(cons, NULL);
	DPT("pthread join");
	assert(ctp_queue_empty(que, NULL));
	size_t size;
	void* ret = ctp_queue_destroy(que, &size, NULL);
	assert(size == 0);
	assert(ret == NULL);
	DPT("success");
}

struct arg4
{
	size_t pop_size;
	size_t push_size;
	atomic_bool consume_up;
	ctp_queue* que;
};

static 
void* thd_consume4(void* varg)
{
	struct arg4* arg = varg;
	ctp_queue* que = arg->que;
	size_t pop_size = arg->pop_size;
	size_t push_size = arg->push_size;
	
	for (size_t i = 0; i < pop_size; ++i)
	{
		int ec;
		int* n = ctp_queue_wait_pop(que, &ec);
		assert(ec == 0);
		if (i < push_size)
		{
			assert(*n == (int)i);
			if (i == push_size-1)
				atomic_store(&arg->consume_up, true);
		}
		else
		{
			assert(n == NULL);
		}
	}
	return NULL;
}

static 
void* thd_producer4(void* varg)
{
	struct arg4* arg = varg;
	ctp_queue* que = arg->que;
	size_t push_size = arg->push_size;
	int ec;
	for (size_t i = 0; i < push_size; ++i)
	{
		int* n = malloc(sizeof(int));
		*n = i;
		ctp_queue_push(que, n, &ec);
		assert(ec == 0);
	}
	return NULL;
}

static
void test_queue_wait_terminate_base(size_t push_size, size_t pop_size)
{
	assert(pop_size > push_size);
	// ignore err
	{
		ctp_queue* que = ctp_queue_create(NULL);
		pthread_t consumer, producer;
		struct arg4* arg = malloc(sizeof(struct arg4));
		arg->que = que;
		arg->push_size = push_size;
		arg->pop_size = pop_size;
		arg->consume_up = false;

		pthread_create(&consumer, NULL, thd_consume4, arg);
		pthread_create(&producer, NULL, thd_producer4, arg);

		DPT("pthread create");
		pthread_join(producer, NULL);

		while(!atomic_load(&arg->consume_up)) { }

		ctp_queue_all_wait_terminate(que, NULL);
		
		pthread_join(consumer, NULL);
		DPT("pthread join");
		free(arg);
		assert(ctp_queue_empty(que, NULL));
		ctp_queue_destroy(que, NULL, NULL);
		DPT("success");
	}
}

// ai gen start
// 共享参数结构体
struct mt_test_arg {
    ctp_queue* que;
    size_t push_size;        // 每个生产者产生的元素数量
    size_t pop_size;         // 每个消费者尝试弹出的元素数量
    int thread_idx;          // 线程索引
    int producer_count;      // 生产者总数
    int consumer_count;      // 消费者总数
    atomic_int* items_consumed;  // 已消费的元素计数
    atomic_int* producers_done;  // 已完成的生产者计数
    atomic_int* consumers_done;  // 已完成的消费者计数
    atomic_bool termination_started;  // 是否已开始终止等待
};

// 消费者线程函数
static void* thd_consume_mt(void* varg)
{
    struct mt_test_arg* arg = varg;
    ctp_queue* que = arg->que;
    size_t pop_size = arg->pop_size;
    int total_items = arg->producer_count * arg->push_size;
    
    for (size_t i = 0; i < pop_size; ++i)
    {
        // 如果已经通知终止，则不再继续
        if (atomic_load(&arg->termination_started))
            break;
            
        int ec;
        int* n = ctp_queue_wait_pop(que, &ec);
        assert(ec == 0);
        
        if (n != NULL) {
            // 验证值在有效范围内
            assert(*n >= 0 && *n < total_items);
            free(n);  // 释放内存
            
            // 递增已消费的元素数量
            atomic_fetch_add(arg->items_consumed, 1);
        } else {
            // 如果队列已终止或所有元素已消费完毕，可能返回NULL
            int consumed = atomic_load(arg->items_consumed);
            int producers_done = atomic_load(arg->producers_done);
            
            // 如果所有生产者已完成且所有元素已消费，返回NULL是正确的
            assert(consumed >= total_items || producers_done >= arg->producer_count || 
                   atomic_load(&arg->termination_started));
        }
    }
    
    // 标记此消费者已完成
    atomic_fetch_add(arg->consumers_done, 1);
    return NULL;
}

// 生产者线程函数
static void* thd_produce_mt(void* varg)
{
    struct mt_test_arg* arg = varg;
    ctp_queue* que = arg->que;
    size_t push_size = arg->push_size;
    int thread_idx = arg->thread_idx;
    
    // 生产特定范围的数据
    for (size_t i = 0; i < push_size; ++i)
    {
        if (atomic_load(&arg->termination_started))
            break;
            
        int* n = malloc(sizeof(int));
        *n = thread_idx * push_size + i;  // 确保每个元素有唯一值
        
        int ec;
        ctp_queue_push(que, n, &ec);
        assert(ec == 0);
    }
    
    // 标记此生产者已完成
    atomic_fetch_add(arg->producers_done, 1);

    return NULL;
}

// 多线程测试函数
static void test_queue_wait_terminate_multi_thread(
    size_t push_size,      // 每个生产者产生的元素数量
    size_t pop_size,       // 每个消费者尝试弹出的元素数量
    int producer_count,    // 生产者线程数量
    int consumer_count)    // 消费者线程数量
{
    // 创建队列
    ctp_queue* que = ctp_queue_create(NULL);
    
    // 创建线程数组
    pthread_t* consumers = malloc(consumer_count * sizeof(pthread_t));
    pthread_t* producers = malloc(producer_count * sizeof(pthread_t));
    
    // 创建参数数组
    struct mt_test_arg** consumer_args = malloc(consumer_count * sizeof(struct mt_test_arg*));
    struct mt_test_arg** producer_args = malloc(producer_count * sizeof(struct mt_test_arg*));
    
    // 共享计数器
    atomic_int items_consumed = 0;
    atomic_int producers_done = 0;
    atomic_int consumers_done = 0;
    atomic_bool termination_started = false;
    
    // 创建并启动消费者线程
    for (int i = 0; i < consumer_count; i++) {
        consumer_args[i] = malloc(sizeof(struct mt_test_arg));
        consumer_args[i]->que = que;
        consumer_args[i]->push_size = push_size;
        consumer_args[i]->pop_size = pop_size;
        consumer_args[i]->thread_idx = i;
        consumer_args[i]->producer_count = producer_count;
        consumer_args[i]->consumer_count = consumer_count;
        consumer_args[i]->items_consumed = &items_consumed;
        consumer_args[i]->producers_done = &producers_done;
        consumer_args[i]->consumers_done = &consumers_done;
        consumer_args[i]->termination_started = termination_started;
        
        pthread_create(&consumers[i], NULL, thd_consume_mt, consumer_args[i]);
    }
    
    // 创建并启动生产者线程
    for (int i = 0; i < producer_count; i++) {
        producer_args[i] = malloc(sizeof(struct mt_test_arg));
        producer_args[i]->que = que;
        producer_args[i]->push_size = push_size;
        producer_args[i]->pop_size = pop_size;
        producer_args[i]->thread_idx = i;
        producer_args[i]->producer_count = producer_count;
        producer_args[i]->consumer_count = consumer_count;
        producer_args[i]->items_consumed = &items_consumed;
        producer_args[i]->producers_done = &producers_done;
        producer_args[i]->consumers_done = &consumers_done;
        producer_args[i]->termination_started = termination_started;
        
        pthread_create(&producers[i], NULL, thd_produce_mt, producer_args[i]);
    }
	DPT("pthread create");
    
    // 等待所有生产者完成
    for (int i = 0; i < producer_count; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // 等待消费者消费完所有项目
    while (atomic_load(&items_consumed) < producer_count * push_size) {
        // 短暂休眠以减少CPU使用
        struct timespec ts = {0, 1000000}; // 1ms
        nanosleep(&ts, NULL);
    }
    
    // 标记开始终止
    atomic_store(&termination_started, true);
    
    // 终止所有等待操作
    ctp_queue_all_wait_terminate(que, NULL);
    
    // 等待所有消费者完成
    for (int i = 0; i < consumer_count; i++) {
        pthread_join(consumers[i], NULL);
    }
	DPT("pthread join");
    
    // 验证所有项目都被消费
    assert(atomic_load(&items_consumed) == producer_count * push_size);
    
    // 清理资源
    for (int i = 0; i < consumer_count; i++) {
        free(consumer_args[i]);
    }
    for (int i = 0; i < producer_count; i++) {
        free(producer_args[i]);
    }
    
    free(consumer_args);
    free(producer_args);
    free(consumers);
    free(producers);
    
    ctp_queue_destroy(que, NULL, NULL);
	DPT("success");
}
// ai gen end


int main()
{
	test_push_wait_pop_base();
	test_push_wait_pop_more_thds(1);
	test_push_wait_pop_more_thds(2);
	test_push_wait_pop_more_thds(20);
	test_push_try_pop(2);
	test_push_try_pop(20);
	test_queue_wait_terminate_base(50, 100);
	test_queue_wait_terminate_multi_thread(100, 200, 4, 4);
}
