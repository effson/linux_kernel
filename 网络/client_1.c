#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 8888
#define BUFFLEN 1024

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFLEN];
    int n;

    // 如果没有指定服务器 IP，提示并退出
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 连接服务器
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 发送 "TIME" 请求
    strcpy(buffer, "TIME");
    send(sockfd, buffer, strlen(buffer), 0);

    // 接收服务器返回的时间
    memset(buffer, 0, BUFFLEN);
    n = recv(sockfd, buffer, BUFFLEN, 0);
    if (n > 0) {
        printf("Server time: %s", buffer);  // ctime 末尾自带换行
    } else {
        printf("Failed to receive response from server.\n");
    }

    // 关闭连接
    close(sockfd);
    return 0;
}
