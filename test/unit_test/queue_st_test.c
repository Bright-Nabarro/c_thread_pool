#include <assert.h>
#include <stdlib.h>

#include "ctp/ts_queue.h"

static 
void test_queue_create_destory()
{
	ctp_queue_t* que1 = ctp_queue_create(NULL);
	assert(que1);
	assert(ctp_queue_empty(que1, NULL));
	void* ret1 = ctp_queue_destroy(que1, NULL, NULL);
	assert(ret1 == NULL);

	int n = 1;
	ctp_queue_t* que2 = ctp_queue_create(&n);
	assert(n == 0);
	assert(que2);
	n = 1;
	assert(ctp_queue_empty(que2, &n));
	n = 1;
	size_t size;
	void* ret2 = ctp_queue_destroy(que2, &size, &n);
	assert(n == 0);
	assert(ret2 == NULL);
	assert(size == 0);
}



static 
void test_push_try_pop_base()
{
	// 忽略错误
	ctp_queue_t* que1 = ctp_queue_create(NULL);
	int n1 = 42;
	void* p = &n1;
	ctp_queue_push(que1, p, NULL);
	int* pret = ctp_queue_try_pop(que1, NULL);
	assert(*pret == n1);
	assert(pret == &n1);
	void* ret1 = ctp_queue_destroy(que1, NULL, NULL);
	assert(ret1 == NULL);
	
	// 接收错误
	int ec = 1;
	ctp_queue_t* que2 = ctp_queue_create(&ec);
	int n2 = 24;
	void* p2 = &n2;
	ctp_queue_push(que2, p2, &ec);
	assert(ec == 0);
	int* ret2 = ctp_queue_try_pop(que2, &ec);
	assert(ec == 0);
	assert(*ret2 == n2);
	assert(ret2 == &n2);
	size_t size2 = 1;
	void* des_ret2 = ctp_queue_destroy(que2, &size2, &ec);
	assert(size2 == 0);
	assert(des_ret2 == NULL);
}

static
void test_queue_create_destory_with_ele(size_t push_size)
{
	// ignore err
	{
		ctp_queue_t* que = ctp_queue_create(NULL);
		for (size_t i = 0; i < push_size; ++i)
		{
			int* n = malloc(sizeof(int));
			*n = i;
			ctp_queue_push(que, n, NULL);
		}
		size_t arr_size;
		void** arr = ctp_queue_destroy(que, &arr_size, NULL);
		assert(arr);
		assert(arr_size == push_size);
		for (size_t i = 0; i < arr_size; ++i)
		{
			assert(*(int*)arr[i] == (int)i);
			free(arr[i]);
		}
		free(arr);
	}
	// with error code
    {
        int ec = 1;
        ctp_queue_t* que = ctp_queue_create(&ec);
        assert(ec == 0);
        
        for (size_t i = 0; i < push_size; ++i)
        {
            int* n = malloc(sizeof(int));
            *n = i;
            ctp_queue_push(que, n, &ec);
            assert(ec == 0);
        }
        
        size_t arr_size;
        void** arr = ctp_queue_destroy(que, &arr_size, &ec);
        assert(ec == 0);
        assert(arr);
        assert(arr_size == push_size);
        
        for (size_t i = 0; i < arr_size; ++i)
        {
            assert(*(int*)arr[i] == (int)i);
            free(arr[i]);
        }
        free(arr);
    }
}

static
void test_push_wait_pop_loop(int loop_size)
{
	// 忽略错误
	{
		ctp_queue_t* que = ctp_queue_create(NULL);
		int* array = malloc(loop_size * sizeof(int));
		for (int i = 0; i < loop_size; ++i)
		{
			array[i] = i;
			ctp_queue_push(que, &array[i], NULL);
		}

		for (int i = 0; i < loop_size; ++i)
		{
			int* p = ctp_queue_wait_pop(que, NULL);
			assert(*p == array[i]);
			assert(p == &array[i]);
		}
		assert(ctp_queue_empty(que, NULL));
		free(array);
		size_t size = 123;
		void* ret = ctp_queue_destroy(que, &size, NULL);
		assert(ret == NULL);
		assert(size == 0);
	}
	// 考虑错误
	{
		int ec = 1;
    	ctp_queue_t* que = ctp_queue_create(&ec);
    	assert(ec == 0);
    	int* array = malloc(loop_size * sizeof(int));
    	for (int i = 0; i < loop_size; ++i)
    	{
    	    array[i] = i;
    	    ctp_queue_push(que, &array[i], &ec);
    	    assert(ec == 0);
    	}

    	for (int i = 0; i < loop_size; ++i)
    	{
    	    int* p = ctp_queue_wait_pop(que, &ec);
    	    assert(ec == 0);
    	    assert(*p == array[i]);
    	    assert(p == &array[i]);
    	}
    	assert(ctp_queue_empty(que, &ec));
    	assert(ec == 0);
    	free(array);
    	size_t size = 123;
    	void* ret = ctp_queue_destroy(que, &size, &ec);
    	assert(ret == NULL);
    	assert(size == 0);
    	assert(ec == 0);
	}
}

static
void test_push_pop_destory(size_t push_size, size_t pop_size)
{
	assert(push_size >= pop_size);
	// 忽略错误
	{
		ctp_queue_t* que = ctp_queue_create(NULL);

		for (size_t i = 0; i < push_size; ++i)
		{
			int* n = malloc(sizeof(int));
			*n = i;
			ctp_queue_push(que, n, NULL);
		}

		for (size_t i = 0; i < pop_size; ++i)
		{
			int* n = ctp_queue_wait_pop(que, NULL);
			assert(*n == (int)i);
			free(n);
		}
		
		size_t left;
		void** arr = ctp_queue_destroy(que, &left, NULL);
		assert(arr);
		assert(left == push_size - pop_size);
		for (size_t i = 0; i < left; ++i)
		{
			assert((int)i + (int)pop_size == *(int*)arr[i]);
			free(arr[i]);
		}
		free(arr);

	}

	// 考虑错误
	{
		int ec = 1;
    	ctp_queue_t* que = ctp_queue_create(&ec);
    	assert(ec == 0);

    	for (size_t i = 0; i < push_size; ++i)
    	{
    	    int* n = malloc(sizeof(int));
    	    *n = i;
    	    ctp_queue_push(que, n, &ec);
    	    assert(ec == 0);
    	}

    	for (size_t i = 0; i < pop_size; ++i)
    	{
    	    int* n = ctp_queue_wait_pop(que, &ec);
    	    assert(ec == 0);
    	    assert(*n == (int)i);
    	    free(n);
    	}
    	
    	size_t left;
    	void** arr = ctp_queue_destroy(que, &left, &ec);
    	assert(ec == 0);
    	assert(arr);
    	assert(left == push_size - pop_size);
    	for (size_t i = 0; i < left; ++i)
    	{
    	    assert((int)i + (int)pop_size == *(int*)arr[i]);
    	    free(arr[i]);
    	}
    	free(arr);
	}
}

int main()
{
	test_queue_create_destory();
	test_push_try_pop_base();
	test_queue_create_destory_with_ele(100);
	test_push_wait_pop_loop(100000);
	test_push_pop_destory(100, 20);
}

