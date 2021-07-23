#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "egn_core.h"
#include "egn_inet.h"
#include "egn_util.h"
#include "log.h"
#include "check.h"
const int kBacklog=128;
const int kBufSize=1500;
static volatile bool g_running=true;
void signal_exit_handler(int sig)
{
    g_running=false;
}
typedef struct{
    egn_conn_obj_t parent;
    int msg_count;
    int run_time;
}tcp_echo_conn_t;
void tcp_echo_handler(egn_t *egn,egn_event_t *ev){
    tcp_echo_conn_t *conn=(tcp_echo_conn_t*)ev->data;
    int fd,n=0;
    fd=conn->parent.fd;
    char buffer[kBufSize];
    bool fin=false;
    if(!ev->shut){
        if(conn->parent.repoints&EGNPOINTIN){
            while(true){
                memset(buffer,0,kBufSize);
                int n=read(fd,buffer,kBufSize);
                if(-1==n){
                    if(EINTR==errno||EWOULDBLOCK==errno||EAGAIN==errno){
                        //pass
                    }else{
                        fin=true;
                    }
                    break;
                }
                if(0==n){
                    fin=true;
                    break;
                }
                if(n>0){
                    conn->msg_count++;
                    log_info("%d recv msg: %s",fd,buffer);
                    write(fd,buffer,n);
                    if(conn->msg_count>=conn->run_time){
                        fin=true;
                    }
                }
            }
            if(fin){
                log_info("close %d",fd);
                egn_remove_io(egn,ev,0);
                close(fd);
                egn_del_object(egn,conn);
            }
        }
    }else{
        log_info("io shut %d",fd);
        egn_remove_io(egn,ev,0);
        close(fd);
        egn_del_object(egn,conn);
    }
}
inline tcp_echo_conn_t *create_tcp_conn(egn_t *egn,int fd,egn_event_handler handler){
    tcp_echo_conn_t *conn=nullptr;
    egn_event_t *ev=nullptr;
    int sz1=sizeof(tcp_echo_conn_t);
    int sz2=sizeof(egn_event_t);
    void *addr=nullptr;
    addr=egn_new_object(egn,sz1+sz2);
    if(nullptr==addr){
        return conn;
    }
    conn=(tcp_echo_conn_t*)addr;
    ev=(egn_event_t*)(addr+sz1);
    conn->parent.ev=ev;
    conn->parent.fd=fd;
    ev->data=conn;
    ev->handler=handler;
    log_info("new ev %p",ev);
    return conn;
}
tcp_echo_conn_t* create_tcp_peer_conn(egn_t *egn,int fd,egn_event_handler handler){
    tcp_echo_conn_t *conn=create_tcp_conn(egn,fd,handler);
    egn_event_t *ev=nullptr;
    int points=0;
    if(!conn){
        close(fd);
        return conn;
    }
    conn->run_time=3;
    ev=conn->parent.ev;
    egn_nonblocking(fd);
    points=EGNPOINTIN|EGNPOINTET;
    egn_add_io(egn,ev,points);
    return conn;
}
void tcp_listen_handler(egn_t *egn,egn_event_t *ev){
    tcp_echo_conn_t *listen_conn=(tcp_echo_conn_t*)ev->data;
    int listen_fd=listen_conn->parent.fd;
    sockaddr_storage sock_saddr;
    socklen_t sock_addr_sz=sizeof(sock_saddr);
    int s=-1;
    int ret;
    char ip_str[INET6_ADDRSTRLEN];
    uint16_t port=0;
    memset(ip_str,0,INET6_ADDRSTRLEN);
    if(!ev->shut){
        if(listen_conn->parent.repoints&EGNPOINTIN){
            while((s=accept(listen_fd,(sockaddr*)&sock_saddr,&sock_addr_sz))>=0){
                sockaddr_string(&sock_saddr,&port,ip_str,INET6_ADDRSTRLEN);
                log_info("accept from %s:%d",ip_str,port);
                create_tcp_peer_conn(egn,s,tcp_echo_handler); 
                memset(ip_str,0,INET6_ADDRSTRLEN);
            }
        }
    }else{
        log_info("io shut %d",listen_fd);
        egn_remove_io(egn,ev,0);
        close(listen_fd);
        egn_del_object(egn,listen_conn);
    }
}
tcp_echo_conn_t* create_tcp_listener(egn_t *egn,int family,
            const char*ip,uint16_t port,egn_event_handler handler){
    tcp_echo_conn_t *conn=nullptr;
    egn_event_t *ev=nullptr;
    int points=0;
    int fd=bind_tcp(family,ip,port,kBacklog);
    if(fd<0){
        log_error("fd is negative");
        return conn;
    }
    log_info("listen fd %d",fd);
    conn=create_tcp_conn(egn,fd,handler);
    if(nullptr==conn){
        log_error("conn is nullptr");
        close(fd);
        return conn;
    }
    egn_nonblocking(fd);
    ev=conn->parent.ev;
    points=EGNPOINTIN|EGNPOINTET;
    egn_add_io(egn,ev,points);
    return conn;
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
    const char *ip="0.0.0.0";
    uint16_t port=3345;
    egn_t *egn=egn_new_engine();
    if(nullptr==egn){
        return -1;
    }
    usleep(1000);//sleep 1ms;
    create_tcp_listener(egn,AF_INET,ip,port,tcp_listen_handler);
    while(g_running){
        egn_loop_once(egn,0);
    }
    egn_del_engine(egn);
    return 0;
}
