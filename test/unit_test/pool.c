#include "thd_pool.c"

void job_fn1(void*)
{
}

#define TEST_POOL_CRT_SCALE 20
void thd_pool_with_job_test1()
{
	int err = 0;
	// 使用外部工具检测是否有内存泄漏竞争等等的基本情况
	for (size_t i = 0; i < TEST_POOL_CRT_SCALE; ++i)
	{
		thd_pool* ppl = thd_pool_create(1, &err);
		assert(ppl != nullptr);
		assert(err == 0);
		job* job = thd_pool_job_create(job_fn1, nullptr, &err);
		assert(job != nullptr);
		assert(err == 0);
		thd_pool_add_job(ppl, job, &err);
		assert(err == 0);
		err =thd_pool_destroy(ppl, nullptr);
		assert(err == 0);
	}
}

void thd_pool_with_job_test2()
{
	for (size_t i = 0; i < TEST_POOL_CRT_SCALE; ++i)
	{
		thd_pool* ppl = thd_pool_create(2, nullptr);
		thd_pool_emplace_job(ppl, job_fn1, nullptr, nullptr);
		thd_pool_destroy(ppl, nullptr);
	}
}

void job_fn_int(void* arg)
{
	atomic_int* pn = arg;
	atomic_fetch_add(pn, 1);
}

#define TEST_JOB_SCALE 1'000'000

void test_thd_pool_int1(size_t thd_num)
{
	thd_pool* ppl = thd_pool_create(thd_num, nullptr);
	atomic_int n = 0;
	for (size_t i = 0; i < TEST_JOB_SCALE; ++i)
	{
		thd_pool_emplace_job(ppl, job_fn_int, &n, nullptr);
	}
	thd_pool_destroy(ppl, nullptr);
	assert(n == TEST_JOB_SCALE);
}



int main()
{
	thd_pool_with_job_test1();
	thd_pool_with_job_test2();
	test_thd_pool_int1(2);
	test_thd_pool_int1(10);
}
