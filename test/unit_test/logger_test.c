#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ctp/logger.h"

static
void test_init_close()
{
	{ //sync stdout
		int ec;
		ctp_logger_config_t config;
		ctp_logger_config_default(&config);
		ctp_logger_context_t* context = ctp_logger_init(&config, &ec);
		assert(ec == 0);
		ctp_logger_close(context, &ec);
		assert(ec == 0);
		if (fputs("", stdout) == EOF && ferror(stdout))
		{
			fprintf(stderr, "stdout appears to be closed or invalid\n");
			exit(1);
		}

	}
	{ //async stdout
		int ec;
		ctp_logger_config_t config;
		ctp_logger_config_default(&config);
		config.async = true;
		ctp_logger_context_t* context = ctp_logger_init(&config, &ec);
		assert(ec == 0);
		ctp_logger_close(context, &ec);
		assert(ec == 0);
		if (fputs("", stdout) == EOF && ferror(stdout))
		{
			fprintf(stderr, "stdout appears to be closed or invalid\n");
			exit(1);
		}
	}
	const char* file = "/tmp/test_init_close.log";
	{ //sync file
		int ec;
		ctp_logger_config_t config;
		ctp_logger_config_default(&config);
		config.log_file = file;
		ctp_logger_context_t* context = ctp_logger_init(&config, &ec);
		assert(ec == 0);
		ctp_logger_close(context, &ec);
		assert(ec == 0);
		assert(remove(file) == 0);
	}
	{ //async file
		int ec;
		ctp_logger_config_t config;
		ctp_logger_config_default(&config);
		config.log_file = file;
		config.async = true;
		ctp_logger_context_t* context = ctp_logger_init(&config, &ec);
		assert(ec == 0);
		ctp_logger_close(context, &ec);
		assert(ec == 0);
		assert(remove(file) == 0);
	}
}

static
void test_output(bool async)
{
	int ec;
    ctp_logger_config_t config;
    ctp_logger_config_default(&config);
	config.async = async;
    
	const char* file_name = "/tmp/test_logger_output.log";
    // 设置日志文件路径
    config.log_file = file_name;
    
    // 初始化日志系统
    ctp_logger_context_t* context = ctp_logger_init(&config, &ec);
    assert(ec == 0);
    
    // 准备测试数据和预期结果
    const char* level_names[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    ctp_log_level_t levels[] = {ctp_debug, ctp_info, ctp_warn, ctp_error, ctp_fatal};
    
    // 写入不同级别的日志
    for (int i = 0; i < 5; i++) {
        char test_message[100];
        sprintf(test_message, "This is a %s test message", level_names[i]);
        
        ec = ctp_logger_log(context, levels[i], "%s", test_message);
        assert(ec == 0);
    }
    
    // 关闭日志系统
    ctp_logger_close(context, &ec);
    assert(ec == 0);
    
    // 验证文件是否已创建并包含日志内容
    FILE* file = fopen(file_name, "r");
    assert(file != NULL);
    
    // 读取文件内容进行验证
    char buffer[1024];
    size_t read_size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[read_size] = '\0';
    
    // 验证每个日志级别的消息是否都在文件中
    for (int i = 0; i < 5; i++) {
        char expected[100];
        sprintf(expected, "[%s] This is a %s test message", 
                level_names[i], level_names[i]);
        
        assert(strstr(buffer, expected) != NULL);
    }
    
    assert(fclose(file) == 0);
    
    // 清理：删除测试文件
    assert(remove(file_name) == 0);
}

int main()
{
	test_init_close();
	test_output(false);
	test_output(true);
}
