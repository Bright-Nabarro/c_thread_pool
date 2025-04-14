#pragma once
#include <stdio.h>

#define DPT(msg, ...)  \
	do { \
		fprintf(stderr, "%s: " msg "\n", __func__ __VA_OPT__(,) __VA_ARGS__); \
	} while(0)
