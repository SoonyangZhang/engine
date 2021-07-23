#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>
#include "egn_core.h"
#define EGN_MIN(a,b) (a>b?b:a)
#define INLINE    static inline
int64_t egn_time_millis();

void egn_up_time();
uint32_t egn_relative_millis();

bool egn_memeqzero(const void *data, size_t length);
void* egn_malloc(int sz);
void* egn_realloc(void *mem_address, unsigned int newsize);
void egn_free(void *alloc);

uint16_t egn_hton16(uint16_t x);
uint32_t egn_hton32(uint32_t x);
uint64_t egn_hton64(uint64_t x);

uint16_t egn_ntoh16(uint16_t x);
uint32_t egn_ntoh32(uint32_t x);
uint64_t egn_ntoh64(uint64_t x);
#ifdef __cplusplus
}
#endif
