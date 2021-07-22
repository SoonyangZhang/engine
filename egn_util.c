#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <memory.h>
#include "check.h"
#include "egn_util.h"
//#define EGN_HAVE_CLOCK_MONOTONIC 1
int64_t g_up_millis=-1;
int64_t egn_monotonic_time(time_t sec, int64_t msec){
#if (EGN_HAVE_CLOCK_MONOTONIC)
    struct timespec  ts;

#if defined(CLOCK_MONOTONIC_FAST)
    clock_gettime(CLOCK_MONOTONIC_FAST, &ts);

#elif defined(CLOCK_MONOTONIC_COARSE)
    clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    sec = ts.tv_sec;
    msec = ts.tv_nsec / 1000000;
#endif
    return (int64_t) sec * 1000 + msec;
}
int64_t egn_read_time(){
    struct timeval   tv;
    time_t           sec;
    int64_t msec;
    int64_t egn_current_msec;
    gettimeofday(&tv, NULL);
    sec = tv.tv_sec;
    msec = tv.tv_usec / 1000;
    egn_current_msec=egn_monotonic_time(sec,msec);
    return egn_current_msec;
}
int64_t egn_time_millis(){
    return egn_read_time();
}
void egn_up_time(){
    if(-1==g_up_millis){
        g_up_millis=egn_time_millis();
    }
}
uint32_t egn_relative_millis(){
    int64_t now=egn_time_millis();
    DCHECK(g_up_millis>0);
    DCHECK(now>=g_up_millis);
    return now-g_up_millis;
}
bool egn_memeqzero(const void *data, size_t length)
{
    const unsigned char *p = (const unsigned char*)data;
    static unsigned long zeroes[16]={0};

    while (length > sizeof(zeroes)) {
        if (memcmp(zeroes, p, sizeof(zeroes)))
            return false;
        p += sizeof(zeroes);
        length -= sizeof(zeroes);
    }
    return memcmp(zeroes, p, length) == 0;
}
void* egn_malloc(int sz){
    void *alloc=malloc(sz);
    if(alloc){
        memset(alloc,0,sz);
    }
    return alloc;
}
void* egn_realloc(void *mem_address, unsigned int newsize){
    return realloc(mem_address,newsize);
}
void egn_free(void *alloc){
    free(alloc);
}
