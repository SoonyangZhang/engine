#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
#if defined (WIN32)
#include <winsock2.h>
#include<Ws2tcpip.h>
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif
const char *inet_ntop(int af, const void *src, char* dst, socklen_t size);
int inet_pton(int af, const char* src, void *dst);
#else
#include <netinet/in.h>
#include<arpa/inet.h>
#include <fcntl.h>
#define egn_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)
int bind_tcp(int familiy,const char *ip,uint16_t port,int backlog);
#endif
int sockaddr_init(struct sockaddr_storage* addr,int familiy,const char*ip,uint16_t port);
int sockaddr_string(const struct sockaddr_storage* addr,uint16_t *port,char *dst,socklen_t dst_sz);
#ifdef __cplusplus
}
#endif

