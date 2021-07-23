#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string>
#include "egn_core.h"
#include "egn_inet.h"
#include "egn_util.h"
#include "log.h"
#include "check.h"
enum TcpConnStatus :uint8_t{
    TCP_CONN_CLOSED,
    TCP_CONN_TRYING,
    TCP_CONN_OK,
    TCP_CONN_FAILED,
};
const int kBufSize=1500;
const int gSendBytes=1500*20000;
const int gBufSize=gSendBytes;
const char *hello_world="hello world ";
char g_buffer[gBufSize];
static volatile bool g_running=true;
typedef struct{
    egn_conn_obj_t parent;
    int write_bytes;
    int total_bytes;
    uint8_t status;
}tcp_cient_conn_t;
void signal_exit_handler(int sig)
{
    g_running=false;
}
//default 16384
void log_fd_send_buf(int fd){
    int send_buf_sz=0;
    socklen_t len=sizeof(send_buf_sz);
    int s =getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buf_sz, &len);
    log_info("send buf size %d",send_buf_sz);
}
inline int send_chunk(int fd,const char *buf,int sz){
    int n=0,ret=0;
    n=send(fd,buf,sz,0);
    if(-1==n){
        if(EINTR==errno||EWOULDBLOCK==errno||EAGAIN==errno){
            //normal 
            ret=0;
        }else{
            //error close
            ret=-1;
        }
    }else{
        CHECK(n>=0);
        ret=n;
    }
    return ret;
}
void ayn_client_write(egn_t *egn,tcp_cient_conn_t *conn){
    int fd=conn->parent.fd;
    int offset,a,b,target,n;
    while(true){
        offset=conn->write_bytes%gBufSize;
        a=gBufSize-offset;
        b=conn->total_bytes-conn->write_bytes;
        target=EGN_MIN(a,b);
        n=send_chunk(fd,g_buffer+offset,target);
        if(n>0){
            conn->write_bytes+=n;
            log_info("target %d write bytes: %d",target,n);
        }else if(0==n){
            //write next time
            log_info("write next time: %d",conn->write_bytes);
            break;
        }else if(n<0){
            conn->status=TCP_CONN_CLOSED;
            log_error("write failure");
            break;
        }
        if(conn->write_bytes>=conn->total_bytes){
            conn->status=TCP_CONN_CLOSED;
            break;
        }
    }
}
void asyn_client_handler(egn_t *egn,egn_event_t *ev){
    tcp_cient_conn_t *conn=(tcp_cient_conn_t*)ev->data;
    int fd,n=0,repoints=0,points=0;
    fd=conn->parent.fd;
    bool fin=false;
    char rbuffer[kBufSize];
    if(!ev->shut){
        repoints=conn->parent.repoints;
        log_poller_events(repoints);
        if(repoints&(EGNPOINTERR|EGNPOINTHUP)){
            if(TCP_CONN_TRYING==conn->status){
                conn->status=TCP_CONN_FAILED;
                log_info("%d connect failed",fd);
            }else if(TCP_CONN_OK==conn->status){
                conn->status=TCP_CONN_CLOSED;
                log_info("%d connect error",fd);
            }
        }
        if(repoints&EGNPOINTOUT){
            //in et mode, write until full;
            if(TCP_CONN_TRYING==conn->status){
                conn->status=TCP_CONN_OK;
                int x=conn->total_bytes;
                x=egn_hton32(x);
                int *p=(int*)g_buffer;
                memcpy(p,&x,sizeof(x));
                ayn_client_write(egn,conn);
            }else if(TCP_CONN_OK==conn->status){
                ayn_client_write(egn,conn);
            }
        }
        if(TCP_CONN_OK==conn->status&&(repoints&EGNPOINTIN)){
            while(true){
                memset(rbuffer,0,kBufSize);
                int n=read(fd,rbuffer,kBufSize);
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
                    log_info("%d recv len: %d",fd,n);
                    if(n>0){
                        //pass
                    }else{
                        conn->status=TCP_CONN_CLOSED;
                        log_error("write failure");
                    }
                }
            }
        }
        if(fin||TCP_CONN_OK!=conn->status){
            log_info("close %d",fd);
            egn_remove_io(egn,ev,0);
            close(fd);
            egn_del_object(egn,conn);
            g_running=false;
        }
    }else{
        log_info("io shut %d",fd);
        egn_remove_io(egn,ev,0);
        close(fd);
        egn_del_object(egn,conn);
    }
}
inline tcp_cient_conn_t *create_tcp_conn(egn_t *egn,int fd,egn_event_handler handler){
    tcp_cient_conn_t *conn=nullptr;
    egn_event_t *ev=nullptr;
    int sz1=sizeof(tcp_cient_conn_t);
    int sz2=sizeof(egn_event_t);
    void *addr=nullptr;
    addr=egn_new_object(egn,sz1+sz2);
    if(nullptr==addr){
        return conn;
    }
    conn=(tcp_cient_conn_t*)addr;
    ev=(egn_event_t*)(addr+sz1);
    conn->parent.ev=ev;
    conn->parent.fd=fd;
    ev->data=conn;
    ev->handler=handler;
    return conn;
}
int create_asn_client(egn_t *egn,egn_event_handler handler,const char *serv_ip,uint16_t port){
    int ret=EGN_ERR,fd,points=0;
    tcp_cient_conn_t *conn=nullptr;
    egn_event_t *ev=nullptr;
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr =inet_addr(serv_ip);
    servaddr.sin_port = htons(port);
    if ((fd= socket(AF_INET, SOCK_STREAM, 0)) < 0){
        log_error("could not create fd");
        return ret;
    }
    log_fd_send_buf(fd);
    conn=create_tcp_conn(egn,fd,handler);
    if(nullptr==conn){
        close(fd);
        return ret;
    }
    conn->total_bytes=gSendBytes;
    ev=conn->parent.ev;
    
    egn_nonblocking(fd);
    points=EGNPOINTIN|EGNPOINTET|EGNPOINTOUT;
    if(connect(fd,(struct sockaddr *)&servaddr,sizeof(servaddr)) == -1&&errno!= EINPROGRESS){
        //connect doesn't work, are we running out of available ports ? if yes, destruct the socket
        if (errno == EAGAIN){
            log_error("connect failure");
            egn_del_object(egn,conn);
            close(fd);
            fd=-1;
            return ret;
        }
    }
    ret=egn_add_io(egn,ev,points);
    CHECK(EGN_OK==ret);
    conn->status=TCP_CONN_TRYING;
    return ret;
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
    int ret;
    egn_t *egn=egn_new_engine();
    if(nullptr==egn){
        return -1;
    }
    usleep(1000);//sleep 1ms;
    ret=create_asn_client(egn,asyn_client_handler,ip,port);
    if(EGN_ERR==ret){
        return ret;
    }
    while(g_running){
        egn_loop_once(egn,0);
    }
    egn_del_engine(egn);
    return 0;
}
