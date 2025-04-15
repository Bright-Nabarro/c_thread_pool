#pragma once
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

// 不作为导出库

#define INI_EC(p_ec)                                                           \
	do                                                                         \
	{                                                                          \
		if ((p_ec) == NULL)                                                    \
			break;                                                             \
		*p_ec = 0;                                                             \
	} while (0)

#define ERR_INT(ec, p_ec, next_process)                                        \
	do                                                                         \
	{                                                                          \
		if ((p_ec) == NULL)                                                    \
			break;                                                             \
		if (ec != 0)                                                           \
		{                                                                      \
			*(p_ec) = (ec);                                                    \
			next_process;                                                      \
		}                                                                      \
	} while (0)

#define ERR_PEC(p_ec, next_process)                                            \
	do                                                                         \
	{                                                                          \
		if ((p_ec) == NULL)                                                    \
			break;                                                             \
		if ((*p_ec) != 0)                                                      \
		{                                                                      \
			next_process;                                                      \
		}                                                                      \
	} while (0)

#define ERR_PTR_ERRNO(ptr, p_ec, next_process)                                 \
	do                                                                         \
	{                                                                          \
		if ((p_ec) == NULL)                                                    \
			break;                                                             \
		if ((ptr) == NULL)                                                     \
		{                                                                      \
			*(p_ec) = errno;                                                   \
		}                                                                      \
	} while (0)

#ifdef CTP_DEBUG
#define DPT(msg, ...)                                                          \
	do                                                                         \
	{                                                                          \
		fprintf(stderr, "%s: " msg "\n", __func__ __VA_OPT__(, ) __VA_ARGS__); \
	} while (0)
#define THDPT(msg, ...)                                                        \
	do                                                                         \
	{                                                                          \
		fprintf(stderr, "thd[%ld] %s: " msg "\n", pthread_self(),              \
				__func__ __VA_OPT__(, ) __VA_ARGS__);                          \
	} while (0)

#else
#define DPT(msg, ...)
#define THDPT(msg, ...) 
#endif


