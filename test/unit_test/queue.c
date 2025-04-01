#include "thd_pool.c"

void job_fn1(void*)
{

}

void test_queue_crud1()
{
	queue* que = queue_create(nullptr);
	assert(queue_empty(que));
	job* j1 = thd_pool_job_create(job_fn1, nullptr, nullptr);
	queue_inque(que, j1);
	assert(!queue_empty(que));
	job* rj1 = queue_deque(que);
	job_destroy(rj1);
	assert(rj1 == j1);
	assert(queue_empty(que));
	queue_destroy(que, nullptr);
}

void test_queue_crud2()
{
	const size_t job_num = 100;
	queue* que = queue_create(nullptr);
	job** job_list = malloc(sizeof(job) * job_num);
	for (size_t i = 0; i < job_num; ++i)
	{
		job_list[i] = thd_pool_job_create(job_fn1, nullptr, nullptr);
		queue_inque(que, job_list[i]);
	}

	for (size_t i = 0; i < job_num; ++i)
	{
		job* pjob = queue_deque(que);
		assert(pjob == job_list[i]);
	}

	assert(queue_empty(que));
	
	queue_destroy(que, nullptr);
}

#define TEST_PTR_ERR(ptr, ec)                                                  \
	do                                                                         \
	{                                                                          \
		assert(que != nullptr);                                                \
		assert(ec == 0);                                                       \
	} while (0)

void test_queue_crud1_err()
{
	int ec = 0;
	queue* que = queue_create(&ec);
	TEST_PTR_ERR(que, ec);
	assert(queue_empty(que));
	job* j1 = thd_pool_job_create(job_fn1, nullptr, &ec);
	TEST_PTR_ERR(j1, ec);
	queue_inque(que, j1);
	assert(!queue_empty(que));
	job* rj1 = queue_deque(que);
	TEST_PTR_ERR(rj1, ec);
	job_destroy(rj1);
	assert(rj1 == j1);
	assert(queue_empty(que));
	queue_destroy(que, nullptr);
}

int main()
{
	test_queue_crud1();
	test_queue_crud2();
	test_queue_crud1_err();
	return 0;
}

