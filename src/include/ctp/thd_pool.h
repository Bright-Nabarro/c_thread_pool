#pragma once
#include <stddef.h>
#include <stdbool.h>

struct ctp_thdpool;
typedef struct ctp_thdpool ctp_thdpool;

ctp_thdpool* ctp_thdpool_create(size_t thd_size, int* ec);

void ctp_thdpool_add_job(ctp_thdpool* thp, void (*fn)(void*), void* args,
						 void (*cleanup)(void*), int* ec);

/// 如果不在前调用wait, 则仅仅作为提示
bool ctp_thdpool_empty(ctp_thdpool* thp, int* ec);

//void ctp_thdpool_wait(ctp_thdpool* thp, int* ec);

void ctp_thdpool_destroy(ctp_thdpool* thp, int* ec, int** err_list,
						 size_t* errlist_size);


