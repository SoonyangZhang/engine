#pragma once
#include "log.h"
//enable check
#ifndef NDEBUG
#define DCHECK_IS_ON
#endif
// thank you brother tj & stephen (https://github.com/clibs/unlikely)
#if defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__)
#  define likely(expr) (__builtin_expect(!!(expr), 1))
#  define unlikely(expr) (__builtin_expect(!!(expr), 0))
#else
#  define likely(expr) (1 == !!(expr))
#  define unlikely(expr) (0 == !!(expr))
#endif

// overload if you want your own abort fn - w/e
#ifndef CHECK_ABORT_FUNCTION
// need that prototype def
# include <stdlib.h>
# define CHECK_ABORT_FUNCTION abort
#endif

// check and bail if nope
#ifndef CHECK
# define CHECK(expr) \
if (!likely(expr)) { \
    log_error("%s,%d check failed: %s",__FILE__,__LINE__,#expr);\
    CHECK_ABORT_FUNCTION(); \
}
#endif

// equivalent to CEHCK(false)
#ifndef NOTREACHED
#define NOTREACHED() CHECK(0)
#endif

// alias `NOTREACHED()` because YOLO
#ifndef NOTREACHABLE
#define NOTREACHABLE NOTREACHED
#endif

#ifdef DCHECK_IS_ON
    #ifndef DCHECK
    #define DCHECK(...) CHECK(__VA_ARGS__);
    #endif
#else
    #ifndef DCHECK
    #  define DCHECK(...) while(false){CHECK(__VA_ARGS__);}
    #endif
#endif
