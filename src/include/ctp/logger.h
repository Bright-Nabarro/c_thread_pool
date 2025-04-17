#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef enum
{
	ctp_debug,
	ctp_info,
	ctp_warn,
	ctp_error,
	ctp_fatal,
} ctp_log_level;

typedef struct
{
	const char* log_file;		// 日志文件路径，NULL表示仅控制台输出
	//bool use_colors;			// 是否在控制台使用颜色
	bool async;					// 是否启用异步日志
	size_t msg_max_len;			// 单次输出消息的最大长度
	//int queue_size;			// 异步日志队列大小
	//bool include_timestamp; 	// 是否包含时间戳
	//bool include_file_info; 	// 是否包含文件名
	//size_t max_file_size;		// 单个日志文件最大大小，0表示不限制
	//int max_files;			// 日志文件轮转最大数量
} ctp_logger_config;

struct ctp_logger_context;
typedef struct ctp_logger_context ctp_logger_context;

void ctp_logger_config_default(ctp_logger_config* config);
ctp_logger_context* ctp_logger_init(const ctp_logger_config* config,
										  int* ec);

int ctp_logger_logv(ctp_logger_context* context, ctp_log_level level,
					const char* fmt, va_list args);
int ctp_logger_log(ctp_logger_context* context, ctp_log_level level,
					const char* fmt, ...);
void ctp_logger_close(ctp_logger_context* context, int* ec);

void ctp_logger_global_init(const ctp_logger_config* config);
void ctp_logger_global_log(ctp_log_level level, const char* fmt, ...);
void ctp_logger_global_logv(ctp_log_level level, const char* fmt, va_list args);
void ctp_logger_global_close();
