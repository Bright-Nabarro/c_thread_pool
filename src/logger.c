#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "ctp/thd_pool.h"
#include "ctp/logger.h"
#include "ctp/logger_cvn.h"
#include "ctp/thd_pool.h"
#include "ctp_util.h"

#ifndef NDEBUG
const static bool debug_flag = true;
#else
const static bool debug_flag = true;
#endif

typedef struct ctp_logger_context
{
	bool async;
	FILE* out;
	ctp_thdpool_t* pool;
	size_t buffer_size;
} ctp_logger_context_t;

typedef struct ctp_async_arg
{
	ctp_logger_context_t* context;
	char* msg;
} ctp_async_arg;

const static ctp_logger_config_t default_config = {
	.log_file = NULL,
	.async = false,
	.msg_max_len = 8191,
	//.include_file_info = false,
};

static ctp_logger_config_t global_config = default_config;

static ctp_logger_context_t global_context;

static atomic_bool global_context_initial_flag = false;

static
char* get_ctp_log_level_str(ctp_log_level_t level)
{
	switch(level)
	{
	case ctp_debug:
		return "DEBUG";
	case ctp_info:
		return "INFO";
	case ctp_warn:
		return "WARN";
	case ctp_error:
		return "ERROR";
	case ctp_fatal:
		return "FATAL";
	}
}

static
void ctp_init_logger_context(ctp_logger_context_t* context,
									const ctp_logger_config_t* config,
									int* pec)
{
	context->async = config->async;
	//context->include_file_info = config->include_file_info;
	if (config->log_file == NULL)
	{
		context->out = stdout;
	}
	else
	{
		context->out = fopen(config->log_file, "w");
		ERR_PTR_ERRNO(context->out, pec, return);
	}

	if (context->async)
	{
		context->pool = ctp_thdpool_create(1, pec);
		ERR_PEC(pec, return);
	}

	context->buffer_size = config->msg_max_len+1;
}

static
void ctp_close_logger_context(ctp_logger_context_t* context, int* pec)
{
	if (context->async)
	{
		ctp_thdpool_destroy(context->pool, pec, NULL, NULL);
		ERR_PEC(pec, return);
	}

	if (context->out)
		fclose(context->out);
}

static void ctp_logger_output_string(char* buffer, const ctp_logger_context_t* context,
									 ctp_log_level_t level, const char* fmt, va_list args)
{
	size_t offset = 0, bias;
	// print level
	const char* level_str = get_ctp_log_level_str(level);
	bias = strlen(level_str)+3;
	// 考虑\0
	snprintf(buffer, bias+1, "[%s] ", level_str);
	offset += bias;

	size_t buffer_size = context->buffer_size;
	
	// print user message
	buffer[buffer_size-1] = 0;
	vsnprintf(buffer + offset, buffer_size-2-offset, fmt, args);
}

static
void ctp_logger_sync_output(ctp_logger_context_t* context, char* msg)
{
	fprintf(context->out, "%s\n", msg);
	free(msg);
}

static 
void ctp_logger_async_clean_arg(void* varg)
{
	ctp_async_arg* arg = varg;
	free(arg->msg);
	free(arg);
}

static
void ctp_logger_async_output_callback(void* varg)
{
	ctp_async_arg* arg = varg;
	
	fprintf(arg->context->out, "%s\n", arg->msg);
}

static
void ctp_logger_async_output(ctp_logger_context_t* context, char* msg, int* pec)
{
	ctp_async_arg* arg = malloc(sizeof(ctp_async_arg));
	ERR_PTR_ERRNO(arg, pec, return);
	arg->context = context;
	arg->msg = msg;
	ctp_thdpool_add_job(context->pool, ctp_logger_async_output_callback, arg,
						ctp_logger_async_clean_arg, pec);
}

/****
 * INTERFACE IMPLEMENT
 ****/

int ctp_logger_logv(ctp_logger_context_t* context, ctp_log_level_t level,
					const char* fmt, va_list args)
{
	if (level == ctp_debug && !debug_flag)
		return 0;

	int ec = 0;
	char* buffer = calloc(1, context->buffer_size);
	ERR_PTR_ERRNO(buffer, &ec, return ec);
	ctp_logger_output_string(buffer, context, level, fmt, args);
	
	if (!context->async)
	{
		ctp_logger_sync_output(context, buffer);
	}
	else
	{
		ctp_logger_async_output(context, buffer, &ec);
		ERR_PEC(&ec, return ec);
	}
		
	if (level == ctp_error || level == ctp_fatal)
	{
		fflush(context->out);
	}

	return ec;

}

void ctp_logger_config_default(ctp_logger_config_t* config)
{
	*config = default_config;
}

ctp_logger_context_t* ctp_logger_init(const ctp_logger_config_t* config, int* pec)
{
	INI_EC(pec);
	ctp_logger_context_t* context = malloc(sizeof(ctp_logger_context_t));
	ERR_PTR_ERRNO(context, pec, return NULL);
	ctp_init_logger_context(context, config, pec);
	ERR_PEC(pec, return NULL);

	return context;
}

void ctp_logger_close(ctp_logger_context_t* context, int* pec)
{
	INI_EC(pec);
	ctp_close_logger_context(context, pec);
	free(context);
}

int ctp_logger_log(ctp_logger_context_t* context, ctp_log_level_t level,
					const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ec = ctp_logger_logv(context, level, fmt, args);
	va_end(args);
	return ec;
}

void ctp_logger_global_init(const ctp_logger_config_t* config)
{
	memcpy(&global_context, config, sizeof(ctp_logger_config_t));
	ctp_init_logger_context(&global_context, &global_config, NULL);
	atomic_store(&global_context_initial_flag, true);
}

void ctp_logger_global_logv(ctp_log_level_t level, const char* fmt, va_list args)
{
	ctp_logger_logv(&global_context, level, fmt, args);
}

void ctp_logger_global_log(ctp_log_level_t level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ctp_logger_global_logv(level, fmt, args);
	va_end(args);
}

void ctp_logger_global_close()
{
	ctp_close_logger_context(&global_context, NULL);
}

void ctp_log(ctp_log_level_t level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (!atomic_load(&global_context_initial_flag))
	{
		atomic_store(&global_context_initial_flag, true);
		ctp_init_logger_context(&global_context, &global_config, NULL);
	}

	ctp_logger_logv(&global_context, level, fmt, args);
	va_end(args);
}

