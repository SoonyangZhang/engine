#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <memory.h>
#include "check.h"
#include "egn_epoll.h"
#include "egn_util.h"
typedef struct{
    struct epoll_event  *event_list;
    int epfd;
    int n;
}egn_epoll_t;
const char * epoll_event_str[]={
    "epoll_none",
    "epoll_in",
    "epoll_pri",
    "epoll_out",
    "epoll_err",
    "epoll_hup",
    "epoll_nval",
    "epoll_rdnorm",
    "epoll_rdband",
    "epoll_wrnorm",
    "epoll_wrband",
    "epoll_msg",
    "epoll_none",
    "epoll_none",
    "epoll_rdhup",
};
static char epoll_event_buf[256];
static void update_epoll_event_buf(int event){
    memset(epoll_event_buf,0,sizeof(epoll_event_buf));
    int n=sizeof(epoll_event_str)/sizeof(epoll_event_str[0]);
    int off=0;
    for(int i=0;i<n;i++){
        int v=(1<<i);
        if(event&v){
            const char *str=epoll_event_str[i+1];
            int len=strlen(str);
            memcpy(epoll_event_buf+off,str,len);
            off+=len;
            epoll_event_buf[off]=' ';
            off+=1;
        }
    }
}
int egn_epoll_init(egn_t *egn,int n){
    int success=EGN_ERR;
    egn_epoll_t *priv=(egn_epoll_t*)egn->priv;
    if(EGN_PRIV_LEN<sizeof(egn_epoll_t)){
        log_info("epoll size %d",sizeof(egn_epoll_t));
        CHECK(0);
    }
    memset(priv,0,sizeof(egn_epoll_t));
    priv->epfd=-1;
    struct epoll_event *event_list=0;
    int epfd=0;
    epfd=epoll_create1(0);
    if(-1==epfd){
        goto error;
    }
    event_list=(struct epoll_event*)egn_malloc(n*sizeof(struct epoll_event));
    if(0==event_list){
        close(epfd);
        goto error;
    }
    priv->epfd=epfd;
    priv->event_list=event_list;
    priv->n=n;
    success=EGN_OK;
error:
    return success;
}
void egn_epoll_done(egn_t *egn){
    egn_epoll_t *priv=(egn_epoll_t*)egn->priv;
    if(priv->epfd>=0){
        close(priv->epfd);
        priv->epfd=-1;
    }
    if(priv->n){
        egn_free(priv->event_list);
        priv->event_list=0;
        priv->n=0;
    }
}
/*
EPOLLERR, EPOLLHUP:  epoll_wait(2) will always wait for such events;
*/
#define _points_to_events(points,events) do{\
    if(points&EGNPOINTIN){ events|=EPOLLIN ;} \
    if(points&EGNPOINTOUT){ events|=EPOLLOUT ;} \
    if(points&EGNPOINTERR){ events|=EPOLLERR ;} \
    if(points&EGNPOINTHUP){ events|=EPOLLHUP ;} \
    if(points&EGNPOINTRDHUP){ events|=EPOLLRDHUP ;}\
    if(points&EGNPOINTET){ events|=EPOLLET ;} \
}while(0)
#define _events_to_points(events,points) do{\
    if(events&EPOLLIN)      { points|=EGNPOINTIN;} \
    if(events&EPOLLOUT)     { points|=EGNPOINTOUT;} \
    if(events&EPOLLERR)     { points|=EGNPOINTERR;} \
    if(events&EPOLLHUP)     { points|=EGNPOINTHUP;} \
    if(events&EPOLLRDHUP)   { points|=EGNPOINTRDHUP;} \
}while(0)
int egn_epoll_add_event(egn_t *egn,egn_event_t *ev,int points,int flags){
    egn_epoll_t *priv=(egn_epoll_t*)egn->priv;
    int  op,fd;
    struct epoll_event  ee;
    egn_conn_obj_t *conn=NULL;
    conn=ev->data;
    DCHECK(conn);
    fd=conn->fd;
    if(ev->active){
        op=EPOLL_CTL_MOD;
    }else{
        op=EPOLL_CTL_ADD;
    }
    ee.events=points;
    ee.data.ptr=conn;
    if (epoll_ctl(priv->epfd, op,fd, &ee) == -1) {
        log_warn("epoll_ctl(%d, %d) failed", op,fd);
        return EGN_ERR;
    }
    log_info("epoll_ctl(%d, %d) success", op,fd);
    ev->active=1;
    return EGN_OK;
}
int egn_epoll_del_event(egn_t *egn,egn_event_t *ev,int points,int flags){
    egn_epoll_t *priv=(egn_epoll_t*)egn->priv;
    int  op,fd;
    struct epoll_event ee;
    egn_conn_obj_t *conn=NULL;
    conn=ev->data;
    DCHECK(conn);
    fd=conn->fd;
    if(1==ev->active){
        op=EPOLL_CTL_DEL;
        ee.events = 0;
        ee.data.ptr = NULL;
        if (epoll_ctl(priv->epfd,op,fd, &ee) == -1) {
            log_warn("epoll_ctl(%d, %d) failed", op,fd);
            return EGN_ERR;
        }
    }
    log_info("epoll_ctl(%d, %d) success", op,fd);
    ev->active=0;
    return EGN_OK;
}
void egn_epoll_process_event(egn_t *egn,int timeout){
    int nfds=0,i=0;
    egn_epoll_t *priv=(egn_epoll_t*)egn->priv;
    egn_event_t *ev=NULL;
    egn_conn_obj_t *conn=NULL;
    memset(priv->event_list,0,priv->n*sizeof(struct epoll_event));
    nfds=epoll_wait(priv->epfd,priv->event_list,priv->n,timeout);
    for(i=0;i<nfds;i++){
        conn=priv->event_list[i].data.ptr;
        conn->repoints=priv->event_list[i].events;
        ev=conn->ev;
        ev->handler(egn,ev);
        conn->repoints=0;
    }
}
