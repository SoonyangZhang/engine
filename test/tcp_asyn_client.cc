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
const char *hello_world="hello world ";
static volatile bool g_running=true;
void signal_exit_handler(int sig)
{
    g_running=false;
}
typedef struct{
    egn_conn_obj_t parent;
    int write_c;
    int read_c;
    uint8_t status;
}tcp_cient_conn_t;
inline int say_hello(int fd,int count){
    int n=0;
    std::string msg(hello_world);
    msg=msg+std::to_string(count);
    n=write(fd,msg.data(),msg.size());
    return n;
}
void asyn_client_handler(egn_t *egn,egn_event_t *ev){
    tcp_cient_conn_t *conn=(tcp_cient_conn_t*)ev->data;
    int fd,n=0,repoints=0,points=0;
    fd=conn->parent.fd;
    char buffer[kBufSize];
    bool fin=false;
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
            if(TCP_CONN_TRYING==conn->status){
                conn->status=TCP_CONN_OK;
                n=say_hello(fd,conn->write_c);
                if(n>0){
                    points=EGNPOINTIN|EGNPOINTET;
                    int ret=egn_mod_io(egn,ev,points);
                    log_info("egn_mod_io %d",ret);
                    conn->write_c++;
                }else{
                    conn->status=TCP_CONN_CLOSED;
                    log_error("write failure");
                }
            }
        }
        if(TCP_CONN_OK==conn->status&&(repoints&EGNPOINTIN)){
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
                    conn->read_c++;
                    log_info("%d recv msg: %s",fd,buffer);
                    n=say_hello(fd,conn->write_c);
                    if(n>0){
                        conn->write_c++;
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
    conn=create_tcp_conn(egn,fd,handler);
    if(nullptr==conn){
        close(fd);
        return ret;
    }
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
