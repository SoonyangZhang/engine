#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    const char* serv_ip = "0.0.0.0";
    const int serv_port = 3345;
    
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(serv_ip);
    servaddr.sin_port = htons(serv_port);
    
    // open a stream socket
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("could not create socket\n");
        return 1;
    }

    // TCP is connection oriented, a reliable connection
    // **must** be established before any data is exchanged
    if (connect(sock, (struct sockaddr*)&servaddr,
                sizeof(servaddr)) < 0) {
        printf("could not connect to server\n");
        return 1;
    }
    
    // send

    // data that will be sent to the server
    const char* data_to_send = "hello server this is a test message";
    send(sock, data_to_send, strlen(data_to_send), 0);
    printf("send msg: %s\n",data_to_send);
    // receive
    
    int n = 0;
    int len = 0, maxlen =1500;
    char buffer[maxlen];
    char* pbuffer = buffer;
    
    // will remain open until the server terminates the connection
    n = recv(sock, pbuffer, maxlen, 0);
    pbuffer += n;
    maxlen -= n;
    len += n;
    buffer[len] = '\0';
    printf("received: '%s'\n", buffer);
    
    
    // close the socket
    close(sock);
    return 0;
}
