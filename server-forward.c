#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
int main(){
    int server_socket=socket(AF_INET, SOCK_STREAM, 0);//AF_INET地址 SOCK_STREAM是TCP协议
    if (server_socket==-1){
        perror("socket error");
        exit(1);
    }
    struct sockaddr_in server_addr;// sockaddr_in 是专门为 IPv4 设计的地址结构体 服务器自己的
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family=AF_INET;//确定为ipv4;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);//别挑剔，只要是发到这台电脑上的请求（不管是通过 Wi-Fi、网线还是本地回环），我全接了
    server_addr.sin_port=htons(8080);
    if(bind(server_socket,(struct sockaddr*)&server_addr,sizeof(server_addr))==-1){//bind()用于绑定地址与 socket
        perror("bind error");
        exit(1);
    }
    if(listen(server_socket,5)==-1){
        perror("listen error");
        exit(1);
    }
    while(1){
        struct sockaddr_in client_addr;//客户端
        socklen_t client_addr_size=sizeof(client_addr);//大小
        int client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_size);//接收客户端的请求
        if (client_sock == -1) {
            perror("accept error");
            continue;
        }
        printf("浏览器运行了，IP地址: %s\n", inet_ntoa(client_addr.sin_addr));//把网络里的二进制 IP，转换成人类能看懂的字符串 IP。 传参是结构体 
        char buffer[1024];
        memset(buffer,0,sizeof(buffer));//清除内存
        read(client_sock,buffer,sizeof(buffer)-1);
         printf("--- 浏览器原始请求 ---\n%s\n--------------------\n", buffer);//GET /index.html HTTP/1.1
        char method[16];//干什么
        char path[256];// 存 "/index.html"
        memset(method,0,sizeof(method));
        memset(path,0,sizeof(path));
        sscanf(buffer,"%s %s",method,path);//分割空格 method 是get path是index.html
        char filename[256];
        if(strcmp(path,"/")==0){// http://localhost:8080/a.html path a.html
            strcpy(path,"index.html");
        }else{
            strcpy(filename,path+1);// path :/index.html +1 变成 index.html
        }
        FILE *file=fopen(filename,"r");
        if (file==NULL){
            char not_found[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html; charset=utf-8\r\n\r\n<h1>404 没找到文件</h1>";
            write(client_sock,not_found,strlen(not_found));
        }else{
            char ok_header[]="HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
            write(client_sock,ok_header,strlen(ok_header));
            char file_buf[1024];
            size_t bytes_read;
            while(bytes_read=fread(file_buf,1,sizeof(file_buf),file)>0){// fread 函数 第一个参数 传入的数组 第二个 每次传多少个字节 传入数组的大小 传的东西 如果录完了 循环结束
                write(client_sock,file_buf,bytes_read);
            }
            fclose(file);
        }
        close(client_sock);
    }
    close(server_socket);//关闭服务器端
    return 0;
}