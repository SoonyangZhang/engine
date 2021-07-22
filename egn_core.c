#include <string.h>
#include "check.h"
#include "egn_core.h"
#include "egn_util.h"
#include "egn_epoll.h"
#include "log.h"
int _egn_rb_insert_event(struct rb_root *root,egn_event_t *ev){
    uintptr_t key=(uintptr_t)ev;
    struct rb_node **tmp = &(root->rb_node), *parent =0;
    if(0==ev){
        return EGN_ERR;
    }
    while(*tmp){
        egn_event_t *entry=rb_entry(*tmp,egn_event_t, rb_node);
        uintptr_t entry_key=(uintptr_t)entry;
        parent=*tmp;
        if(key<entry_key){
            tmp = &((*tmp)->rb_left);
        }else if(key>entry_key){
            tmp = &((*tmp)->rb_right);
        }else{
            return EGN_ERR;
        }
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&ev->rb_node, parent, tmp);
    rb_insert_color(&ev->rb_node, root);
    return EGN_OK;
}
int _egn_rb_remove_event(struct rb_root *root,egn_event_t *ev){
    struct rb_node *node=&ev->rb_node;
    if(true==egn_memeqzero(node,sizeof(*node))){
        log_warn("event %p was removed before",ev);
        return EGN_ERR;
    }
    rb_erase(&ev->rb_node, root);
    memset(node,0,sizeof(*node));
    return EGN_OK;
}
int _egn_rb_insert_timer(struct rb_root *root,egn_event_t *ev){
    egn_timer_obj_t *inner=0;
    uint32_t key;
    struct rb_node **tmp = &(root->rb_node), *parent =0;
    if(0==ev){
        return EGN_ERR;
    }
    inner=(egn_timer_obj_t*)ev->data;
    key=inner->future;
    while(*tmp){
        egn_event_t *entry=rb_entry(*tmp,egn_event_t, rb_node);
        inner=(egn_timer_obj_t*)entry->data;
        uint32_t entry_key=inner->future;
        parent=*tmp;
        if(key<entry_key){
            tmp = &((*tmp)->rb_left);
        }else{
            tmp = &((*tmp)->rb_right);
        }
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&ev->rb_node, parent, tmp);
    rb_insert_color(&ev->rb_node, root);
    return EGN_OK;
}
egn_event_t* _egn_rb_find_timer(struct rb_root *root,uint32_t key){
    struct rb_node *rbnode = root->rb_node;
    egn_timer_obj_t *inner=0;
    while (rbnode!=NULL)
    {
        egn_event_t *entry=rb_entry(rbnode,egn_event_t, rb_node);
        inner=(egn_timer_obj_t*)entry->data;
        uint32_t entry_key=inner->future;
        if (key < entry_key)
            rbnode = rbnode->rb_left;
        else if (key >entry_key)
            rbnode = rbnode->rb_right;
        else
            return entry;
    }
    return NULL;
}
egn_t*  egn_new_engine(){
    egn_t *egn=0;
    egn=(egn_t*)egn_malloc(sizeof(egn_t));
    if(egn){
        TAILQ_INIT(&egn->event_list);
        int ret=egn_epoll_init(egn,32);
        if(EGN_OK!=ret){
            egn_free(egn);
            egn=NULL;
        }
    }
    return egn;
}
void egn_del_engine(egn_t *egn){
    egn_event_t *ev=NULL;
    egn_timer_obj_t *obj=NULL;
    do{
        ev=TAILQ_FIRST(&egn->event_list);
        if(ev){
        ev->shut=1;
        egn_remove_io_tailq(egn,ev);
        ev->handler(egn,ev);
        }
    }while(ev);
    while(egn->ev_timer_min){
        ev=egn->ev_timer_min;
        obj=ev->data;
        egn_remove_timer(egn,ev);
        ev->timedout=0;
        ev->shut=1;
        ev->handler(egn,ev);
    }
    egn_epoll_done(egn);
    CHECK(0==egn->io_many);
    CHECK(0==egn->timer_many);
    //CHECK(0==egn->obj_many);
    egn_free(egn);
}
void *egn_new_object(egn_t *egn,int sz){
    void *obj=egn_malloc(sz);
    if(obj){
        egn->obj_many++;
    }
    return obj;
}
void egn_del_object(egn_t *egn,void *obj){
    if(obj){
        egn->obj_many--;
        egn_free(obj);
    }
}
int egn_add_io_tailq(egn_t *egn,egn_event_t *ev){
    int ret=EGN_ERR;
    if(ev&&true==egn_memeqzero(&ev->tailq_entry,sizeof(ev->tailq_entry))){
        TAILQ_INSERT_TAIL(&egn->event_list,ev,tailq_entry);
        egn->io_many++;
        ret=EGN_OK;
    }
    return ret;
}
int egn_remove_io_tailq(egn_t *egn,egn_event_t *ev){
    if(true==egn_memeqzero(&ev->tailq_entry,sizeof(ev->tailq_entry))){
        log_warn("event %p was removed before",ev);
    }else{
        TAILQ_REMOVE(&egn->event_list,ev,tailq_entry);
        egn->io_many--;
        memset(&ev->tailq_entry,0,sizeof(ev->tailq_entry));
    }
    return EGN_OK;
}
int egn_add_timer(egn_t *egn,egn_event_t *ev){
    int ret=EGN_ERR;
    egn_timer_obj_t *timer=ev->data;
    egn_timer_obj_t *timer_min=NULL;
    struct rb_node *node=NULL;
    if((ev->timer_set==0)&&timer){
        ev->timer_set=1;
        _egn_rb_insert_timer(&egn->timer_root,ev);
        egn->timer_many++;
        ret=EGN_OK;
        if(NULL==egn->ev_timer_min){
            egn->ev_timer_min=ev;
        }
        timer_min=egn->ev_timer_min->data;
        if(timer_min->future>timer->future){
            egn->ev_timer_min=ev;
        }else if(timer_min->future==timer->future){
            node=rb_first(&egn->timer_root);
            egn->ev_timer_min=rb_entry(node,egn_event_t,rb_node);
        }
    }
    return ret;
}
int egn_remove_timer(egn_t *egn,egn_event_t *ev){
    int ret=EGN_ERR;
    struct rb_node *node=NULL;
    if(1==ev->timer_set){
        ret=_egn_rb_remove_event(&egn->timer_root,ev);
        ev->timer_set=0;
        egn->timer_many--;
        if(ev==egn->ev_timer_min){
            node=rb_first(&egn->timer_root);
            if(node){
                egn->ev_timer_min=rb_entry(node,egn_event_t,rb_node);
            }else{
                egn->ev_timer_min=NULL;
            }
        }
        ret=EGN_OK;
    }
    return ret;
}
int _egn_find_time_delta(egn_t *egn,uint32_t now){
    int delta=-1;
    if(egn->ev_timer_min){
        DCHECK(egn->timer_many>0);
        egn_timer_obj_t *obj=egn->ev_timer_min->data;
        if(obj->future>=now){
            delta=obj->future-now;
        }else{
            delta=0;
        }
    }
    return delta;
}
void _egn_process_timer(egn_t *egn,uint32_t now){
    egn_event_t *ev=egn->ev_timer_min;
    egn_timer_obj_t *obj=NULL;
    struct rb_node *node=NULL;
    //void *ev_vec[EGN_VEC_INIT_SZ];
    int i=0;
    int pos=0;
    if(NULL==ev||(obj=ev->data)->future>now){
        return;
    }
    // method 1, if a timer event is triggered, and in its callback to cancel
    // another timer event, thus can not be done.
/*
    while(true){
        pos=0;
        node=rb_first(&egn->timer_root);
        for (i=0;i<EGN_VEC_INIT_SZ;i++){
            if(node){
                ev=rb_entry(node,egn_event_t,rb_node);
                obj=ev->data;
                if(obj->future<=now){
                    ev_vec[pos]=ev;
                    pos++;
                }else{
                    break;
                }
            }else{
                break;
            }
            node=rb_next(node);
        }
        if(pos>0){
            for(i=0;i<pos;i++){
                ev=ev_vec[i];
                egn_remove_timer(egn,ev);
                ev->timedout=1;
            }
            for(i=0;i<pos;i++){
                ev=ev_vec[i];
                ev->handler(egn,ev);
            }
        }
        
        if(pos<EGN_VEC_INIT_SZ){
            break;
        }
    }
*/
    //mothod 2
    //trigger timer event one by one.
    while(egn->ev_timer_min){
        ev=egn->ev_timer_min;
        obj=ev->data;
        if(obj->future<=now){
            egn_remove_timer(egn,ev);
            ev->timedout=1;
            ev->handler(egn,ev);
        }else{
            break;
        }
    }

}
void egn_process_timer_test(egn_t *egn,uint32_t now){
    _egn_process_timer(egn,now);
}
int egn_add_io(egn_t *egn,egn_event_t *ev,int points){
    int ret=EGN_ERR;
    ret=egn_add_io_tailq(egn,ev);
    if(EGN_OK!=ret){
        log_error("add io failed %p",ev);
    }
    if(EGN_OK==ret){
        ret=egn_epoll_add_event(egn,ev,points,0);
        if(EGN_OK!=ret){
            egn_remove_io_tailq(egn,ev);
        }
    }
    return ret;
}
int egn_remove_io(egn_t *egn,egn_event_t *ev,int points){
    int ret=egn_epoll_del_event(egn,ev,points,0);
    if(EGN_OK==ret){
        egn_remove_io_tailq(egn,ev);
    }
    return ret;
}
void egn_loop(egn_t *egn){
    egn_loop_once(egn,0);
}
void egn_loop_once(egn_t *egn,int timeout_ms){
    int now=0;
    int stop_ts=-1;
    bool wait_flag=true;
    now=egn_relative_millis();
    if(timeout_ms>=0){
        stop_ts=now+timeout_ms;
    }
    while(wait_flag){
        int wait_ms=-1;
        now=egn_relative_millis();
        int timer_delta=_egn_find_time_delta(egn,now);
        if(stop_ts>=0){
            int v=stop_ts-now;
            wait_ms=v>0?v:0;
        }
        if(timer_delta>=0){
            // has timer event
            if(wait_ms>=0){
                wait_ms=EGN_MIN(wait_ms,timer_delta);
            }else{
                wait_ms=timer_delta;
            }
        }
        egn_epoll_process_event(egn,wait_ms);
        now=egn_relative_millis();
        _egn_process_timer(egn,now);
        if((stop_ts>=0&&now>=stop_ts)||egn->stop){
            wait_flag=false;
        }
    }
}
