#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "egn_core.h"
#include "egn_inet.h"
#include "egn_util.h"
#include "log.h"
#include "check.h"
typedef struct{
    egn_timer_obj_t parent;
    int last_time;
    int count;
    int gap_ms;
    int run_time;
} timer_count_t;
void count_timer_handler(egn_t *egn,egn_event_t *ev){
    int delta=0;
    timer_count_t *obj=(timer_count_t*)ev->data;
    uint32_t now=egn_relative_millis();
    obj->parent.future=now+obj->gap_ms;
    if(ev->timedout){
        if(obj->last_time!=0){
            delta=now-obj->last_time;
        }
        obj->last_time=now;
        log_info("run time %d,%d",obj->count,delta);
        obj->count++;
        if(obj->count<obj->run_time){
            egn_add_timer(egn,ev);
        }
    }
    if(ev->shut){
        log_info("shut");
    }
}
timer_count_t *create_count_timer(egn_t *egn,int gap_ms,
                            int run_time,egn_event_handler handler){
    
    timer_count_t *count_timer=nullptr;
    egn_event_t *ev=nullptr;
    if(0==run_time){
        return count_timer;
    }
    void *addr=egn_new_object(egn,sizeof(timer_count_t)+sizeof(egn_event_t));
    count_timer=(timer_count_t*)addr;
    ev=(egn_event_t*)(addr+sizeof(timer_count_t));
    count_timer->gap_ms=gap_ms;
    count_timer->run_time=run_time;
    count_timer->parent.ev=ev;
    ev->data=count_timer;
    ev->handler=handler;
    egn_add_timer(egn,ev);
    return count_timer;
}
static volatile bool g_running=true;
void signal_exit_handler(int sig)
{
    g_running=false;
}
__attribute__((constructor)) void init_before_main()
{
    egn_up_time();
}
int main(){
    signal(SIGTERM, signal_exit_handler);
    signal(SIGINT, signal_exit_handler);
    signal(SIGHUP, signal_exit_handler);//ctrl+c
    signal(SIGTSTP, signal_exit_handler);//ctrl+z
    egn_t *egn=egn_new_engine();
    usleep(1000);//slep 1ms;
    timer_count_t *count=create_count_timer(egn,500,10,count_timer_handler);
    while(g_running){
        egn_loop_once(egn,0);
    }
    if(count){
        egn_del_object(egn,count);
    }
    egn_del_engine(egn);
}
