#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdint.h>
#include "rbtree.h"
#include "queue.h"

#define EGN_PRIV_LEN 32

#define EGN_OK 0
#define EGN_ERR -1

#define EGN_VEC_INIT_SZ 2

enum EGN_POINTS{
  EGNPOINTIN      = (int) (1U <<  0),
  EGNPOINTOUT     = (int) (1U <<  2),
  EGNPOINTERR     = (int) (1U <<  3),
  EGNPOINTHUP     = (int) (1U <<  4),
  EGNPOINTNVAL    = (int) (1U <<  5),
  EGNPOINTRDHUP   = (int) (1U << 13),
  EGNPOINTET = (int) (1U << 31)
};

TAILQ_HEAD(egn_event_list_s, egn_event_s);
typedef struct egn_event_s egn_event_t;
typedef struct egn_event_list_s egn_event_list_t;
typedef struct{
    char priv[EGN_PRIV_LEN];
    egn_event_list_t event_list;
    struct rb_root timer_root;
    egn_event_t *ev_timer_min;
    int timer_many;
    int io_many;
    int obj_many;
    uint8_t stop:1;
}egn_t;

typedef void (*egn_event_handler)(egn_t *egn,egn_event_t *ev);
struct egn_event_s{
    void *data;
    egn_event_handler handler;
    union{
        struct  rb_node rb_node;
        TAILQ_ENTRY(egn_event_s) tailq_entry;
    };
    uint8_t timedout:1;
    uint8_t timer_set:1;
    uint8_t shut:1;
    uint8_t active:1;
};
typedef struct{
    egn_event_t *ev;
    int fd;
    int repoints;
}egn_conn_obj_t;
typedef struct{
    egn_event_t *ev;
    uint32_t future;
}egn_timer_obj_t;

egn_t* egn_new_engine();
void egn_del_engine(egn_t *egn);

void *egn_new_object(egn_t *egn,int sz);
void egn_del_object(egn_t *egn,void *obj);

int egn_add_io_tailq(egn_t *egn,egn_event_t *ev);
int egn_remove_io_tailq(egn_t *egn,egn_event_t *ev);

int egn_add_timer(egn_t *egn,egn_event_t *ev);
int egn_remove_timer(egn_t *egn,egn_event_t *ev);

int egn_add_io(egn_t *egn,egn_event_t *ev,int points);
int egn_remove_io(egn_t *egn,egn_event_t *ev,int points);

void egn_loop(egn_t *egn);
void egn_loop_once(egn_t *egn,int timeout_ms);

void egn_process_timer_test(egn_t *egn,uint32_t now);
#ifdef __cplusplus
}
#endif
