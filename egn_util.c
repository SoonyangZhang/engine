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

// Convert |x| from host order (little endian) to network order (big endian).
#if defined(__clang__) || \
    (defined(__GNUC__) && \
     ((__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || __GNUC__ >= 5))
INLINE uint16_t byte_swap16(uint16_t val){
    return __builtin_bswap16(val);
}
INLINE uint32_t byte_swap32(uint32_t val){
    return __builtin_bswap32(val);
}
INLINE uint64_t byte_swap64(uint64_t val){
    return __builtin_bswap64(val);
}
#else
//https://sites.google.com/site/rajboston1101/my-page/c---bits-manipulation/swapping
//https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
INLINE uint16_t byte_swap16(uint16_t val){
     return (val << 8) | (val >> 8 );
}
INLINE uint32_t byte_swap32(uint32_t val){
    val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF ); 
    return (val << 16) | (val >> 16);
}
INLINE uint64_t byte_swap64(uint64_t val){
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL ) | ((val >> 8) & 0x00FF00FF00FF00FFULL );
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL ) | ((val >> 16) & 0x0000FFFF0000FFFFULL );
    return (val << 32) | (val >> 32);
}
#endif


uint16_t egn_hton16(uint16_t x){
    return byte_swap16(x);
}
uint32_t egn_hton32(uint32_t x){
    return byte_swap32(x);
}
uint64_t egn_hton64(uint64_t x){
    return byte_swap64(x);
}

uint16_t egn_ntoh16(uint16_t x){
    return egn_hton16(x);
}
uint32_t egn_ntoh32(uint32_t x){
    return egn_hton32(x);
}
uint64_t egn_ntoh64(uint64_t x){
    return egn_hton64(x);
}
