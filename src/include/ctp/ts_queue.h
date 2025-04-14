#pragma once
#include <stdbool.h>
#include <stddef.h>

struct ctp_queue;
typedef struct ctp_queue ctp_queue;

/**
 * @brief Function to create a thread safe queue
 */
ctp_queue* ctp_queue_create(int* ec);

/**
 * @brief Function to destroy a queue and clean up resources
 * @note 1. After calling destroy, ensure no additonal push ro pop are perfroming.
 * @note 2. Before calling, ensure no wait_pop operation still waiting.
 * @param left_node_size left resources size in queue 
 * @return resources left in queue
 */
void** ctp_queue_destroy(ctp_queue* pque, size_t* left_node_size, int* ec);

void ctp_queue_push(ctp_queue* pque, void* value, int* ec);
void* ctp_queue_try_pop(ctp_queue* pque, int* ec);
void* ctp_queue_wait_pop(ctp_queue* pque, int* ec);

/**
 * @brief only serves as a hint during concurrent operations
 */
bool ctp_queue_empty(ctp_queue* pque, int* ec);

/**
 * @brief Function to cancel all wait_pop operations
 * @note 1. Cannot guarantee that all wait_pop operations will \ 
 * 			complete after function returns
 * @note 2. wait_pop will not block after being called \
 * 			unless wait_reset is invoked first
 */
void ctp_queue_all_wait_terminate(ctp_queue* pque, int* ec);

void ctp_queue_wait_reset(ctp_queue* pque);
