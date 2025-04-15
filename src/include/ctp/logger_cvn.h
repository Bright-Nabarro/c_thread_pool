#pragma once
#include "logger.h"

void ctp_log(ctp_log_level level, const char* fmt, ...);

#define LOG_DEBUG(fmt, ...) \
	ctp_log(ctp_debug, fmt __VA_OPT__(,) __VA_ARGS__);

#define LOG_INFO(fmt, ...) \
	ctp_log(ctp_info, fmt __VA_OPT__(,) __VA_ARGS__);

#define LOG_WARN(fmt, ...) \
	ctp_log(ctp_warn, fmt __VA_OPT__(,) __VA_ARGS__);

#define LOG_ERROR(fmt, ...) \
	ctp_log(ctp_error, fmt __VA_OPT__(,) __VA_ARGS__);

#define LOG_FATAL(fmt, ...) \
	ctp_log(ctp_fatal, fmt __VA_OPT__(,) __VA_ARGS__);

