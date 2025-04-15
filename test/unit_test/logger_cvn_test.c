#include "ctp/logger_cvn.h"

int main()
{
	ctp_log(ctp_info, "hello world %s", __FILE__);
	LOG_INFO("ctp_logger_global_log canbe used directly with uninitialized");
	LOG_DEBUG("this is a debug message");
	LOG_WARN("use logger cvn may cause naming conflicts");
	LOG_ERROR("variable argument farward is an error operation in c");
	LOG_FATAL("hello world");

}
