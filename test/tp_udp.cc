//Reference: https://blog.csdn.net/chen_jianjian/article/details/50750202
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "egn_core.h"
#include "egn_inet.h"
#include "egn_util.h"
#include "log.h"
#include "check.h"
const int kBufSize=2000;
static volatile bool g_running=true;
void signal_exit_handler(int sig)
{
    g_running=false;
}
inline void tp_send_msg(struct msghdr *msg,struct sockaddr_in *dst_addr){
    struct sockaddr_in *src_addr=(sockaddr_in*)msg->msg_name;
    int fd=bind_udp_addr((struct sockaddr *)src_addr,sizeof(*src_addr),true);
    if(fd){
        sendto(fd, msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len, 0, (struct sockaddr *)dst_addr,sizeof(*dst_addr));
        close(fd);
    }
}
inline void tp_udp_handle_msg(int fd,struct msghdr *msg){
    struct cmsghdr  *cmsg=nullptr;
    char ip_str[INET6_ADDRSTRLEN];
    uint16_t src_port=0,ori_dst_port=0;
    struct sockaddr_in *src_addr=(sockaddr_in*)msg->msg_name;
    struct sockaddr_in  destination;
    memset(&destination,0,sizeof(destination));
    memset(ip_str,0,INET6_ADDRSTRLEN);
    sockaddr_string((const struct sockaddr_storage*)src_addr,&src_port,ip_str,(socklen_t)INET6_ADDRSTRLEN);
    log_info("recv from %s:%d",ip_str,src_port);
    for(cmsg=CMSG_FIRSTHDR(msg);cmsg;cmsg=CMSG_NXTHDR(msg,cmsg)){
        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVORIGDSTADDR){
            memcpy(&destination,CMSG_DATA(cmsg),sizeof(destination));
            memset(ip_str,0,INET6_ADDRSTRLEN);
            sockaddr_string((const struct sockaddr_storage*)&destination,&ori_dst_port,ip_str,(socklen_t)INET6_ADDRSTRLEN);
            log_info("origin dst %s:%d",ip_str,ori_dst_port);
            break;
        }
    }
    tp_send_msg(msg,&destination);
}
void udp_conn_handler(egn_t *egn,egn_event_t *ev){
    egn_conn_obj_t *conn=(egn_conn_obj_t*)ev->data;
    int fd,n=0;
    fd=conn->fd;
    bool fin=false;
    char buffer[kBufSize];
    sockaddr_storage sock_saddr;
    socklen_t sock_addr_sz=sizeof(sock_saddr);
    struct msghdr   msg;
    char    ctl_buf[64];
    struct iovec iov[1];
    if(!ev->shut){
        log_info("in event");
        if(conn->repoints&EGNPOINTIN){
            while(true){
                memset(buffer,0,kBufSize);
                msg.msg_name = &sock_saddr;
                msg.msg_namelen =sock_addr_sz;
                msg.msg_control =ctl_buf;
                msg.msg_controllen =sizeof(ctl_buf);
                iov[0].iov_base=buffer;
                iov[0].iov_len =kBufSize;
                msg.msg_iov = iov;
                msg.msg_iovlen = 1;
                n=recvmsg(fd,&msg,0);
                if(-1==n){
                    if(EINTR==errno||EWOULDBLOCK==errno||EAGAIN==errno){
                        //pass
                    }else{
                        log_info("recvmsg err");
                        fin=true;
                    }
                    break;
                }
                if(0==n){
                    log_info("recvmsg ret zero");
                    fin=true;
                    break;
                }
                msg.msg_iov[0].iov_len=n;
                tp_udp_handle_msg(fd,&msg);
            }
        }
        if(fin||conn->repoints&(EGNPOINTERR|EGNPOINTHUP)){
            log_info("udp fd error %d",(uint32_t)fin);
            egn_remove_io(egn,ev,0);
            close(fd);
            egn_del_object(egn,conn);
        }
    }else{
        log_info("io shut %d",fd);
        egn_remove_io(egn,ev,0);
        close(fd);
        egn_del_object(egn,conn);
    }
}
inline egn_conn_obj_t *create_udp_obj(egn_t *egn,int fd,egn_event_handler handler){
    egn_conn_obj_t *conn=nullptr;
    egn_event_t *ev=nullptr;
    int sz1=sizeof(egn_conn_obj_t);
    int sz2=sizeof(egn_event_t);
    void *addr=nullptr;
    addr=egn_new_object(egn,sz1+sz2);
    if(nullptr==addr){
        return conn;
    }
    conn=(egn_conn_obj_t*)addr;
    ev=(egn_event_t*)(addr+sz1);
    conn->ev=ev;
    conn->fd=fd;
    ev->data=conn;
    ev->handler=handler;
    log_info("new ev %p",ev);
    return conn;
}
egn_conn_obj_t* create_udp_conn(egn_t *egn,int family,
            const char*ip,uint16_t port,egn_event_handler handler){
    egn_conn_obj_t *conn=nullptr;
    egn_event_t *ev=nullptr;
    int points=0;
    int fd=bind_udp(family,ip,port,true);
    if(fd<0){
        log_error("bind failed");
        return conn;
    }
    conn=create_udp_obj(egn,fd,handler);
    if(nullptr==conn){
        log_error("conn null");
        close(fd);
        return conn;
    }
    egn_nonblocking(fd);
    ev=conn->ev;
    points=EGNPOINTIN|EGNPOINTET;
    egn_add_io(egn,ev,points);
    return conn;
}
__attribute__((constructor)) void init_before_main()
{
    egn_up_time();
}
int main(int argc, char *argv[]){
    signal(SIGTERM, signal_exit_handler);
    signal(SIGINT, signal_exit_handler);
    signal(SIGHUP, signal_exit_handler);//ctrl+c
    signal(SIGTSTP, signal_exit_handler);//ctrl+z
    const char *ip="0.0.0.0";
    uint16_t port=2233;
    egn_t *egn=egn_new_engine();
    if(nullptr==egn){
        return -1;
    }
    usleep(1000);//sleep 1ms;
    create_udp_conn(egn,AF_INET,ip,port,udp_conn_handler);
    while(g_running){
        egn_loop_once(egn,0);
    }
    egn_del_engine(egn);
    return 0;
}
