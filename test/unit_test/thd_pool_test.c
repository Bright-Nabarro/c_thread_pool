#include <assert.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "ctp/thd_pool.h"

static
void test_thdpool_create_destroy(size_t thd_size)
{
	// ignore error
	{
		size_t n = 0;
		ctp_thdpool* thdpool1 = ctp_thdpool_create(thd_size, NULL);
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
		ctp_thdpool* thdpool1 = ctp_thdpool_create(thd_size, &err);
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
		ctp_thdpool* thp = ctp_thdpool_create(thd_size, NULL);
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
		ctp_thdpool* thp = ctp_thdpool_create(thd_size, NULL);
		atomic_int cnt = 0;
		for (size_t i = 0; i < job_size; ++i)
			ctp_thdpool_add_job(thp, fn2, &cnt, NULL, NULL);
		ctp_thdpool_destroy(thp, NULL, NULL, NULL);
		assert(cnt == job_size);
	}
}

int main()
{
	test_thdpool_create_destroy(3);
	test_thdpool_add_job_base(1, 2);
	test_thdpool_add_job_base(4, 100); 
	test_thdpool_add_job_valid(1, 100);
	test_thdpool_add_job_valid(10, 100000);
}
