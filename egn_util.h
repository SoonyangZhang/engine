#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include "egn_core.h"
#define EGN_MIN(a,b) (a>b?b:a)
int64_t egn_time_millis();

void egn_up_time();
uint32_t egn_relative_millis();

bool egn_memeqzero(const void *data, size_t length);
void* egn_malloc(int sz);
void* egn_realloc(void *mem_address, unsigned int newsize);
void egn_free(void *alloc);
#ifdef __cplusplus
}
#endif
