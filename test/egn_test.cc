#include <iostream>
#include <stdio.h>
#include <time.h>
#include "egn_core.h"
#include "egn_util.h"
#include "log.h"
#include "check.h"
void test_timer_event_cb(egn_t *egn,egn_event_t *ev){
    egn_timer_obj_t *obj=(egn_timer_obj_t*)ev->data;
    if(ev->timedout){
        printf("time out %p,%p,%d\n",ev,obj,obj->future);
    }
    if(ev->shut){
        printf("shut %p,%p,%d\n",ev,obj,obj->future);
    }
}
void test_timer_rb_tree(){
    srand(time(0));
    int i=0;
    egn_t *egn=egn_new_engine();
    egn_timer_obj_t *timer_data_array[10];
    egn_timer_obj_t *timer_data=nullptr;
    struct rb_node *node=nullptr;
    printf("----init---\n");
    for(i=0;i<10;i++){
        egn_event_t *ev=(egn_event_t*)egn_malloc(sizeof(egn_event_t));
        timer_data=(egn_timer_obj_t*)egn_malloc(sizeof(egn_timer_obj_t));
        uint32_t future=rand()%100;
        timer_data->future=future;
        ev->data=timer_data;
        ev->handler=test_timer_event_cb;
        timer_data->ev=ev;
        timer_data_array[i]=timer_data;
        egn_add_timer(egn,ev);
        printf("%p,%p,%d\n",ev,timer_data,future);
    }
    printf("----iter---\n");
    node=rb_first(&egn->timer_root);
    while(node){
        egn_event_t *ev=rb_entry(node,egn_event_t, rb_node);
        node=rb_next(node);
        egn_timer_obj_t *timer_data=(egn_timer_obj_t*)ev->data;
        uint32_t future=timer_data->future;
        printf("%p,%p,%d\n",ev,timer_data,future);
    }
    printf("-----process time-----\n");
    egn_process_timer_test(egn,timer_data_array[4]->future);
    printf("-----remove-----\n");
    node=rb_first(&egn->timer_root);
    if(node){
    egn_event_t *ev=rb_entry(node,egn_event_t, rb_node);
    timer_data=(egn_timer_obj_t*)ev->data;
    printf("%p,%p,%d\n",ev,timer_data,timer_data->future);
    egn_remove_timer(egn,ev);
    int ret=egn_remove_timer(egn,ev);
    if(EGN_OK!=ret){
        log_info("remove failed");
    }
    }
    printf("-----min-----\n");
    if(egn->ev_timer_min){
        timer_data=(egn_timer_obj_t*)egn->ev_timer_min->data;
        printf("%p,%p,%d\n",egn->ev_timer_min,timer_data,timer_data->future);
    }
    printf("----iter---\n");
    node=rb_first(&egn->timer_root);
    while(node){
        egn_event_t *ev=rb_entry(node,egn_event_t, rb_node);
        node=rb_next(node);
        egn_timer_obj_t *timer_data=(egn_timer_obj_t*)ev->data;
        uint32_t future=timer_data->future;
        printf("%p,%p,%d\n",ev,timer_data,future);
    }
    
    /*printf("-----remove all-----\n");
    node=rb_first(&egn->timer_root);
    while(node){
        egn_event_t *ev=rb_entry(node,egn_event_t, rb_node);
        node=rb_next(node);
        egn_remove_timer(egn,ev);
    }*/
    node=rb_first(&egn->timer_root);
    if(!node){
        printf("tree is emptyed\n");
    }else{
        printf("tree is not emptyed %d\n",egn->timer_many);
    }
    egn_del_engine(egn);
    for(i=0;i<10;i++){
        egn_event_t *ev=timer_data_array[i]->ev;
        egn_free(ev);
        egn_free(timer_data_array[i]);
        timer_data_array[i]=nullptr;
    }
}
__attribute__((constructor)) void init_before_main()
{
    egn_up_time();
}
int main(){
    #ifndef NDEBUG
    log_info("debug mode");
    #endif
    test_timer_rb_tree();
    return 0;
}
