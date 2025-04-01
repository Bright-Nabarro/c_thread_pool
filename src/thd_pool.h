#pragma once
#include <stddef.h>
#include <errno.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#    define THD_POOL_SUPPORT_C23 1
#else
#    define THD_POOL_SUPPORT_C23 0
#endif

#if !THD_POOL_SUPPORT_C23
#include <stdbool.h>
#endif

typedef struct _thd_pool thd_pool;
struct _job;

// Forward declaration of the _job structure
struct _job;

typedef enum thd_pool_error_code : int
{
	thd_pool_success = 0,
	thd_pool_srch	= ESRCH,
	thd_pool_again = EAGAIN,
	thd_pool_nomem = ENOMEM,
	thd_pool_busy = EBUSY,
	thd_pool_inval = EINVAL,
	thd_pool_deadlk = EDEADLK,
} thd_pool_error_code;

const char* thd_pool_error_str(thd_pool_error_code error_code);
void thd_pool_perror(const char* msg, thd_pool_error_code error_code);
/**
 * Function to create a job
 * @param func: Function pointer representing the job's task
 * @param param: Pointer to the parameter passed to the job's task
 * @param error_code: 0 indicates success, pass nullptr to ignore
 * @return: A pointer to the created job; if an error occurs, returns nullptr
 */
struct _job* thd_pool_job_create(void (*func)(void*), void* param,
								 thd_pool_error_code* error_code);

/**
 * Function to add a job directly to the thread pool
 * @param func: Function pointer representing the job's task
 * @param param: Pointer to the parameter passed to the job's task
 * @param error_code: 0 indicates success, pass nullptr to ignore
 */
void thd_pool_emplace_job(thd_pool* ppl, void (*func)(void*), void* param,
						  thd_pool_error_code* error_code);
/*
 * Function to create a thread pool
 * @param thd_size: Number of threads to create in the pool
 * @param error_code: 0 indicates success, pass nullptr to ignore
 * @return: Pointer to the created thread pool; if an error occurs, returns
 * nullptr
 */
thd_pool* thd_pool_create(size_t thd_size, thd_pool_error_code* error_code);

/*
 * Function to destroy a thread pool and clean up resources
 * @param pool: Pointer to the thread pool to destroy
 * @param error_codes: every thread error code 
 * @return 0 if no error happens process and all threads success, \
 * return -1 if error happends in a threads, otherwise return error_code 
 */
int thd_pool_destroy(thd_pool* pool, thd_pool_error_code** error_codes);

/*
 * Function to add a pre-created job to the thread pool
 * @param job: Pointer to the job to be added
 * @param error_code: 0 indicates success, pass nullptr to ignore
 */
void thd_pool_add_job(thd_pool* ppl, struct _job* job, thd_pool_error_code* error_code);
