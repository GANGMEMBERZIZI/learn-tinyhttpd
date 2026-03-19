#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)x)//由于isspace 传入的参数是unsigned char的 强制转换是为了解决传入的字符有特殊字符 
#define SERVER_STRING "Server: zizihttpd/0.1.0\r\n"

void accept_request(void *arg);

void bad_request(int);

void cat(int, FILE *);

void cannot_execute(int);

void error_die(const char *);

//执行cig脚本
void execute_cgi(int, const char *, const char *, const char *);

int get_line(int, char *, int);


void headers(int, const char *);


void not_found(int);


void serve_file(int, const char *);


int startup(u_short *);

void unimplemented(int);

void serve_file(int client, const char *filename){
    FILE* resource=NULL;
    int numchars=1;
    char buf[1024];
    buf[0]='A'; buf[1]='\0'; //防止没初始化 自动生成换行符
    while((numchars>0)&&(strcmp("\n",buf))){
        numchars=get_line(client,buf,sizeof(buf));//读取路径
    }
    resource=fopen(filename,"r");
    if(resource==NULL){
        not_found(client);
    }else{
        header(client,filename);//header 这个跟html的不一样 协议层和内容层
        //header
// Host: 192.168.1.100:80    <-- 告诉服务器你访问的是哪个地址
// User-Agent: Mozilla/5.0   <-- 告诉服务器你用的是什么浏览器
// Content-Type: text/html   <-- 告诉对方接下来的数据是网页还是图片
// Content-Length: 45        <-- 告诉对方接下来的数据有多少个字节
        cat(client,resource);//读取文件内容
    }
    fclose(resource);
}
void error_die(const char *sc){
    perror(sc);
    exit(1);
}
void cat(int client,FILE* resource){
    char buf[1024];
    fgets(buf,sizeof(buf),resource);
    while(!feof(resource)){//检测是否是末尾
        send(client,buf,strlen(buf),0);
        fgets(buf, sizeof(buf), resource);//自动接着读下一行
    }
}
int get_line(int sock,char *buf,int size){//只能读取一行 
    int i=0;
    char c='\0';//空字符
    int n;//recv的返回值
    while((i<size-1)&&(c!='\n')){
        n=recv(sock,&c,1,0);// sock 管道编号 1是每次读取一个字节 返回值 -1 0 1 异常 正常关闭 正常
        if(n>0){
            if(c=='\r'){
                n=recv(sock,&c,1,MSG_PEEK);//偷看 不移动指针
                if((n>0)&&(c=='\n')){//如果是\n 表示是windows的换行 \r是mac的 统一\n 
                    n=recv(sock,&c,1,0);//真的读取 跳过了\r 
                }else{
                    c='\n';//直接赋值
                }
            }
            buf[i]=c;//录入缓冲区
            i++;
        }else{
            c = '\n';//直接结束循环
        }
        buf[i]='\0';
    }
    return i;
}
void header(int client,char *filename){
    char buf[1024];
    (void) filename;//因为没有用到默认html
    strcpy(buf,"HTTP/1.1 200 OK\r\n");
    send(client,buf,strlen(buf),0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}
void not_found(int client)//没有找到资源
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "your request because the resource specified\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "is unavailable or nonexistent.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}
void bad_request(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "<P>Your browser sent a bad request, ");
 send(client, buf, sizeof(buf), 0);
 sprintf(buf, "such as a POST without a Content-Length.\r\n");
 send(client, buf, sizeof(buf), 0);
}
void unimplemented(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");//赋值 一句一句发防止buf溢出 \r\n防止浏览器乱码
 send(client, buf, strlen(buf), 0);//send函数 发送
 sprintf(buf, SERVER_STRING);
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-Type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</TITLE></HEAD>\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "</BODY></HTML>\r\n");
 send(client, buf, strlen(buf), 0);
}
void cannot_execute(int client)
{
 char buf[1024];

 sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "Content-type: text/html\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "\r\n");
 send(client, buf, strlen(buf), 0);
 sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
 send(client, buf, strlen(buf), 0);
}
void execute_cgi(int client,const char *path,const char *method,const char *query_string){
    char buf[1024];
    int cgi_input[2];
    int cgi_output[2];//输出管道 0是读取端 1是写入端
    pid_t pid;//通用的int
    int status;
    int i;
    char c;
    int numchars=1;
    int content_length = -1;
    buf[0] = 'A'; buf[1] = '\0';//默认字符
    if(strcasecmp(method,"GET")==0){
        while((numchars>0)&&(strcmp("\n",buf))){
            numchars=get_line(client,buf,sizeof(buf));//清除数据
        }
    }else if(strcasecmp(method,"POST")==0){
        numchars=get_line(client,buf,sizeof(buf));//第二行 
        while((numchars>0)&&(strcmp("\n",buf))){
            buf[15]='\0';//获取content-length: 分割成两个新字符串
            if (strcasecmp(buf, "Content-Length:") == 0){//如果读到了
                content_length=atoi(&buf[16]);//atoi 把字符串转换成真正的数
        }
        numchars=get_line(client,buf,sizeof(buf));//没有content-length就一直读
    }
    if(content_length==-1){//没收到
        bad_request(client);
        return ;
    }
}
if(pipe(cgi_output)<0){//建立输出管道
    cannot_execute(client);
    return ;
}
if (pipe(cgi_input) < 0) {//建立输入管道
        cannot_execute(client);
        return;
    }
    if((pid=fork())<0){//分配子进程 返回pid是编号
        cannot_execute(client);
        return;
    }
    sprintf(buf,"HTTP/1.1 200 OK\r\n");
    send(client,buf,strlen(buf),0);
    if(pid==0){
    char meth_env[255];//环境变量
    char query_env[255];
    char length_env[255];
    dup2(cgi_output[1],1);//输出和子进程的写端绑定 printf都会自动流进管道传给父进程  1是STDOUT 
    dup2(cgi_input[0],0);//输入跟子线程的读端绑定  scanf 都会自动从父进程发来的管道里读数据。 0是STDIN
    close(cgi_output[0]);
    close(cgi_input[1]);
    sprintf(meth_env,"REQUEST_METHOD=%s",method);//看是get 还是post
    putenv(meth_env);
    if(strcasecmp(method,"GET")==0){
        sprintf(query_env,"QUERY_STRING=%s",query_string);
        putenv(query_string);
    }else{//POST
        sprintf(length_env,"CONTENT_LENGTH=%d",content_length);
        putenv(length_env);
    }
    execl(path,NULL);//path 绝对路径 子进程原本从父进程那里复制过来的 tinyhttpd 的 C 代码、变量、堆栈，全部被清空。内核根据 path（比如 /var/www/cgi-bin/action.py）找到那个脚本文件，把它的代码加载到子进程的内存里。子进程从新程序的 main 函数（或者是脚本的第一行）开始运行
    exit(0);
    }else{//父进程
        close(cgi_output[1]);//父进程的out只用读端
        close(cgi_input[0]);//只用写端 
        if(strcasecmp(method,"POST")==0){
            // 根据 content_length 读取客户端的信息
            // 并通过 cgi_input 传入子进程的标准输入
            for(int i=0;i<content_length;i++){
                recv(client, &c, 1, 0);//读 content_length
                write(cgi_input[1], &c, 1);//写
            }
        }
        // 通过 cgi_output，获取子进程的标准输出
        // 并将其写入到客户端
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);
        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);//父进程停下脚步，专门等待子进程（CGI 脚本）运行结束。
    }
}
// 特性,静态 (Static),动态 (Dynamic)
// 服务器行为,搬运（Read file & Send）,计算（Run code & Output）
// 文件类型,".html, .css, .png",".cgi, .py, .php"
// CPU 消耗,极低（主要是 IO 消耗）,较高（需要运行进程）
// 交互性,几乎没有,极强（登录、搜索、发帖）
// 在 tinyhttpd 中,直接调用 serve_file,调用 execute_cgi
// 特性,GET (明信片),POST (包裹)
// 主要目的,获取、查询数据,提交、更新、删除数据
// 数据位置,网址（URL）后面,请求体（Body）内部
// 安全性,较低（数据在历史记录中可见）,较高（数据不在地址栏显示）
// 数据大小,受 URL 长度限制（通常 < 2KB）,理论上无限制
// 对服务器影响,幂等（多次执行，结果一样，不改变服务器状态）,非幂等（每执行一次，服务器可能多一条记录）
void accept_request(void *arg){
    int client=(intptr_t)arg;//类型转换 32位和64位系统 arg都是8个字节(client_sock)
    char buf[1024];
    size_t numchars;
    char url[255];
    char path[255];
    char method[512];
    size_t i, j;
    struct stat st;//linux 结构体 用来存储文件的元数据(是否存在 大小等)
    int cgi=0;//0是静态只能录入本地文件 1是动态 进行交互
    char *query_string=NULL;//http://localhost:8080/search?keyword=gemini&lang=zh path是/search query_string(查询参数)keyword=gemini&lang=zh
    numchars=get_line(client,buf,sizeof(buf));//读取socket get_line 这个函数 是通过socket 编号 找到类似通道的地方 然后将网卡的缓冲区的数据 类似 get post host 等等 传入 buf
     i = 0; j = 0;
     while((!ISspace(buf[i]))&&(i<sizeof(method)-1)){// 给终止符留空间
        method[i]=buf[i];//
        i++;
     }
     j=i;//把j赋值到i 准备获取url
     method[i]='\0';
     if(strcasecmp(method,"GET")&&strcasecmp(method,"POST")){
        unimplemented(client);//未实现
        return;
     }
     if(strcasecmp(method,"POST")==0){//POST /login.cgi HTTP/1.1   url是/login.cgi 没有query_string
        cgi=1;//设置动态
     }
     i=0; //清空索引 ->url
     while((ISspace(buf[j]))&&(j<sizeof(buf))){
        j++;
     }
     while((!ISspace(buf[j]))&&(i<sizeof(url)-1)&&(j<sizeof(buf))){
        url[i]=buf[j];
        i++;
        j++;
     }
     url[i]='\0';
     if(strcasecmp(method,"GET")==0){//GET /search?name=zizi HTTP/1.1
        query_string=url;
        while((*query_string!='?')&&(*query_string!='\0'))
        query_string++;
    if(*query_string=='?'){
        cgi=1;
        *query_string='\0';//分割 url query_string
        query_string++;
    }
     }
     sprintf(path,"htdocs%s",url);//字符串拼接 赋值给path 表示文件的实际位置
     if(path[strlen(path)-1]=='/'){//如果没有url 
        strcat(path,"index.html");//默认index.html
     }
     if(stat(path,&st)==-1){
        while((numchars>0)&&strcmp("\n",buf))
        numchars=get_line(client,buf,sizeof(buf));//因为找不到文件接着读取 直到读取完.
        not_found(client);//向客户端发送404
     }else{
        if ((st.st_mode & S_IFMT) == S_IFDIR)//如果是目录 直接默认html st_mode 文件类型 S_IFMT文件权限信息
            strcat(path, "/index.html");
            if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH))
            cgi = 1;//排除get没?的情况
     }
     if(!cgi){//静态模式
        serve_file(client, path);//本地路径传给浏览器
     }else{//动态
        execute_cgi(client, path, method, query_string); 
     }
     close(client);
}
int startup(u_short *port){
    int httpd=0;
    int on = 1;//表示开启
    struct sockaddr_in name;
    httpd=socket(AF_INET,SOCK_STREAM,0);
    if(httpd==-1){
        error_die("socket");
    }
    memset(&name,0,sizeof(name));
    name.sin_family=AF_INET;
    name.sin_port=htons(*port);
    name.sin_addr.s_addr=htonl(INADDR_ANY);//将字符串格式的 ip 地址转换为整型格式的 ip 地址
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) //如果我关掉了服务器，不管那个端口是不是还在 TIME_WAIT 状态，只要我再开启，你就准许我立刻重新霸占这个端口 关掉了等待时间 
    {  
        error_die("setsockopt failed");
    }
    if(bind(httpd,(struct sockaddr*)&name,sizeof(name))==-1){
        error_die("bind");
    }
    if(*port==0){//如果没分配端口号
        socklen_t namelen=sizeof(name);
        if(getsockname(httpd,(struct sockaddr *)&name,&namelen)==-1){//获取系统给 httpd 随机分配的端口号
            error_die("getsockname");
        }
        *port=ntohs(name.sin_port);//把内核从网卡那里拿到的“网络格式”的端口号，强行在 CPU 寄存器里翻转一下，变成你能看懂的正常人类数字
    }
    if(listen(httpd,5)==-1){//监听request 套接字是没法处理的，只能把它放进缓冲区，待当前请求处理完毕后，再从缓冲区中读取出来处理。如果不断有新的请求进来，它们就按照先后顺序在缓冲区中排队，直到缓冲区满。
        error_die("listen");
    }
    return httpd;
}
int main(){
    int server_sock=-1;
    u_short port=8080;
    int client_sock=-1;//客户端通信通道
    struct sockaddr_in client_name;
    socklen_t client_name_len=sizeof(client_name);
    pthread_t newthread;//开辟新线程 开辟线程”就是把“同步阻塞”变成“异步并发”
    server_sock=startup(&port);//该函数完成了socket struct sockaddr_in bind listen 
    printf("httpd running on port %d\n", port);
    while(1){
        client_sock=accept(server_sock,(struct sockaddr*)&client_name,&client_name_len);
        if(client_sock==-1){
            error_die("accept");//自毁程序
        }
        if(pthread_create(&newthread,NULL,(void *)accept_request,(void *)(intptr_t)client_sock)!=0){//创建处理请求  client_sock 值传递 防止竞争 子线程accept_request解析 HTTP 协议，读写网页数据
            perror("pthread_create");
        }
    }
    close(server_sock);
    return 0;
}