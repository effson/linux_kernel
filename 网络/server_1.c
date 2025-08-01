#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define BUFFLEN 1024
#define SERVER_PORT 8888
#define BACKLOG 5
#define PIDNUMB 3

static void handle_connection(int s_s){
    int s_c; // socket for client
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        s_c = accept(s_s, (struct sockaddr *)&client_addr, &addr_len);
        time_t now;
        char buff[BUFFLEN];
        int n = 0;
        memset(buff, 0, BUFFLEN);
        n = recv(s_c, buff, BUFFLEN,0);
        if (n > 0 && !strncmp(buff, "TIME", 4)) {
            memset(buff, 0, BUFFLEN);
            now = time(NULL);
            sprintf(buff, "%24s\r\n", ctime(&now));
            send(s_c, buff, strlen(buff), 0);
            close(s_c);
        }
    }
}

void sig_int(int num) {
    exit(1);
}

int main (int argc, char *argv[]) {
    int s_s; // socket for server
    struct sockaddr_in server_addr;
    /*程序接收到 SIGINT 信号（通常是用户按下 Ctrl+C） 时，
    调用你自定义的函数 sig_int() 来处理该信号*/
    signal(SIGINT, sig_int);

    s_s = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(s_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    listen(s_s, BACKLOG); // 未完成连接请求的队列最大长度（也称“半连接队列”长度）

    pid_t pid[PIDNUMB];
    int i = 0;
    for (i = 0; i < PIDNUMB; i++) {
        pid[i] = fork();
        if (pid[i] == 0) {
            handle_connection(s_s);
        }
    }
    while(1);
    close(s_s);
    return 0;
}
