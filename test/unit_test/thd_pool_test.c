#include <assert.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>

#include "ctp/thd_pool.h"

static
void test_thdpool_create_destroy(size_t thd_size)
{
	// ignore error
	{
		size_t n = 0;
		ctp_thdpool_t* thdpool1 = ctp_thdpool_create(thd_size, NULL);
		assert(thdpool1);
		assert(ctp_thdpool_empty(thdpool1, NULL));
		ctp_thdpool_destroy(thdpool1, NULL, NULL, &n);
		assert(n == 0);
	}
	// handle error
	{
		size_t n = 0;
		int err;
		int* errlist;
		ctp_thdpool_t* thdpool1 = ctp_thdpool_create(thd_size, &err);
		assert(thdpool1);
		assert(ctp_thdpool_empty(thdpool1, &err));
		assert(err == 0);
		ctp_thdpool_destroy(thdpool1, &err, &errlist, &n);
		assert(err == 0);
		assert(errlist == NULL);
		assert(n == 0);
	}
}

static void fn1(void*){}

static
void test_thdpool_add_job_base(size_t thd_size, size_t job_size)
{
	// ignore error
	{
		ctp_thdpool_t* thp = ctp_thdpool_create(thd_size, NULL);
		for (size_t i = 0; i < job_size; ++i)
			ctp_thdpool_add_job(thp, fn1, NULL, NULL, NULL);
		ctp_thdpool_destroy(thp, NULL, NULL, NULL);
	}
}

static void fn2(void* arg)
{
	atomic_int* cnt = arg;
	atomic_fetch_add(cnt, 1);
}

static
void test_thdpool_add_job_valid(size_t thd_size, size_t job_size)
{
	// ignore error
	{
		ctp_thdpool_t* thp = ctp_thdpool_create(thd_size, NULL);
		usleep(100);
		atomic_int cnt = 0;
		for (size_t i = 0; i < job_size; ++i)
			ctp_thdpool_add_job(thp, fn2, &cnt, NULL, NULL);
		ctp_thdpool_destroy(thp, NULL, NULL, NULL);
		assert(cnt == job_size);
	}
}

static void fn_count(void* arg) {
    atomic_int* cnt = arg;
    atomic_fetch_add(cnt, 1);
}

static void fn_sum(void* arg) {
    atomic_int* sum = ((atomic_int**)arg)[0];
    atomic_int* val = ((atomic_int**)arg)[1];
    atomic_fetch_add(sum, atomic_load(val));
}

static void fn_sleep(void* arg) {
    (void)arg;
    usleep(1000); // 1 ms
}

void test_thdpool_comprehensive(size_t thd_size, size_t job_size) {
    ctp_thdpool_t* thp = ctp_thdpool_create(thd_size, NULL);
    usleep(100); // 确保线程池已初始化

    // 1. 简单计数测试
    atomic_int cnt = 0;
    for (size_t i = 0; i < job_size; ++i)
        ctp_thdpool_add_job(thp, fn_count, &cnt, NULL, NULL);

    // 2. 参数传递与累加
    atomic_int sum = 0, val = 2;
    atomic_int* arg_arr[2] = { &sum, &val };
    for (size_t i = 0; i < job_size; ++i)
        ctp_thdpool_add_job(thp, fn_sum, arg_arr, NULL, NULL);

    // 3. 测试耗时任务
    for (size_t i = 0; i < thd_size; ++i)
        ctp_thdpool_add_job(thp, fn_sleep, NULL, NULL, NULL);

    // 4. 多线程并发提交任务

    // 销毁线程池，等待所有任务完成
    ctp_thdpool_destroy(thp, NULL, NULL, NULL);

    // 验证结果
    assert(cnt == job_size);
    assert(sum == (int)job_size * 2);
    assert(concurrent_cnt == 4 * 100);

    printf("All comprehensive threadpool tests passed!\n");
}

int main()
{
	test_thdpool_create_destroy(3);
	test_thdpool_add_job_base(1, 2);
	test_thdpool_add_job_base(4, 100); 
	test_thdpool_add_job_valid(1, 100);
	test_thdpool_add_job_valid(10, 100000);
	test_thdpool_comprehensive(10, 10000);
	test_thdpool_comprehensive(20, 100000);
}
