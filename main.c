/*
用c语言实现简单的http服务器：tiny-c-httpd
实现了GET请求的处理 和 200、404状态码
*/
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <strings.h>

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#define BUF_SIZE 1024*64 //socket接收缓存，64KB
#define SERVER_PORT 10000 //服务器端口

int main(){
    struct sockaddr_in server_sockaddr,client_sockaddr;//声明服务器和客户端的socket存储结构
    int sockfd,client_fd;//socket描述符
    char buf_http_header[BUF_SIZE];//HTTP请求头缓存

    //建立socket链接
    /*
    函数原型：int socket(int family,int type,int protocol)
    参数含义：
        family：对应的就是AF_INET、AF_INET6等。
        type：套接字类型：SOCK_STREAM、SOCK_DGRAM、SOCK_RAW。
        protocol：0
    返回值：
        成功：非负套接字描述符。
        出错：-1
    */
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1) {
        perror("Socket");
        exit(1);
    }
    printf("Socket success! sockfd=%d\n",sockfd);

    //配置socket信息（以sockaddt_in结构体填充socket信息）
    /*
    struct sockaddr_in {
        short sa_family;//地址族，2字节
        unsigned short int sin_port;//端口号，2字节
        struct in_addr sin_addr;//IP地址，4字节，unsigner int
        unsigned char sin_zero[];//填充0以保持与struct sockaddr同样大小，8字节
    }
    */
    server_sockaddr.sin_family = AF_INET;//IPv4
    server_sockaddr.sin_port = htons(SERVER_PORT);//端口。
    server_sockaddr.sin_addr.s_addr = INADDR_ANY;//本主机的任意IP都可以使用
    bzero(&(server_sockaddr.sin_zero),8);//bzero：置字节字符串前n个字节为零

    //服务器bind绑定
    /*
    函数原型：int bind(int sockfd,struct sockaddr *my_addr,int addrlen)
    参数含义：
        sockfd：套接字描述符。
        my_addr：本地地址。
        addrlen：地址长度：
    返回值：
        成功：0
        出错：-1
    */
    if((bind(sockfd,(struct sockaddr *)&server_sockaddr,sizeof(struct sockaddr))) == -1) {
        perror("bind");
        exit(-1);
    }
    printf("Bind success!\n");

    //监听
    /*
    函数原型：int listen(int sockfd,int backlog)
    参数含义：
        sockfd：套接字描述符
        backlog：请求队列中允许的最大请求数，大多数系统默认为20
    返回值：
        成功：0
        出错：-1
    */
    if(listen(sockfd,10) == -1) {//
        perror("listen");
        exit(1);
    }
    printf("服务器初始化成功，正在监听……\n");

    //以上为初始化代码。
    //以下为处理客户端连接代码。

    while(1){

        //等待客户端链接
        /*
        函数原型：int accept(int sockfd,struct sockaddr * addr,socklen_t* addrlen)
        参数含义：
            sockfd：套接字描述符
            addr：客户端地址
            addrlen：地址长度
        返回值：
            成功：0
            出错：-1
        */
        unsigned int sin_size;
        if((client_fd = accept(sockfd,(struct sockaddr *) &client_sockaddr,&sin_size)) == -1) {
            perror("accept");
            exit(1);
        }
        printf("\n接收到请求，客户端地址：%s:%d\n",inet_ntoa(client_sockaddr.sin_addr),htons(client_sockaddr.sin_port));

        //接收并处理客户端的请求
        /*
        函数原型：int recv(int sockfd,void* buf,int len,unsigned int flags)
        参数含义：
            sockfd：套接字描述符
            buf：存放接受数据的缓冲区
            len：数据长度
            flags：一般为0
        返回值：
            成功：接受的字节数
            出错：-1
        */
        unsigned int recvbytes,sendbytes;
        while(1){
            if((recvbytes = recv(client_fd,buf_http_header,BUF_SIZE,0)) == -1) {//如果接受错误则关闭连接
                perror("recv");
                close(client_fd);
                break;
            }
            if(recvbytes==0){
                printf("客户端断开。\n");
                close(client_fd);
                break;
            }
            printf("接收到的字节数: %d\n",recvbytes);
            buf_http_header[recvbytes]=0;
            printf("接收到的内容:\n%s\n",buf_http_header);

            //解析请求类型和URL
            char split_char[] = " ";
            char *splited = NULL;
            splited=strtok(buf_http_header,split_char);
            printf("客户端请求类型: %s\n",splited);//请求类型
            splited=strtok(NULL,split_char);
            char *url = malloc(strlen(splited)+1);
            strcpy(url, splited);
            printf("客户端请求的URL: %s\n",url);//URL

            //读取文件
            int http_status_code;//HTTP状态码，实现了200和404
            if(strcmp(url,"/") == 0){//如果是根目录，修改为index.html
                strcpy(url,"/index.html");
            }
            FILE * fp;
            char www_path[]="./www";
            char *file_path = malloc(strlen(www_path)+strlen(url)+1);
            strcpy(file_path, www_path);
            strcat(file_path, url);
            printf("打开文件: %s\n",file_path);
            if((fp = fopen(file_path, "r"))==NULL){//文件不存在则返回404状态码
                fp = fopen("./www/404.html", "r");//打开404的网页
                http_status_code=404;
            }else{
                http_status_code=200;//文件存在则返回200状态码
            }
            printf("要返回的HTTP状态码: %d\n",http_status_code);
            //获取文件大小
            fseek(fp,0L,SEEK_END);
            unsigned long int file_size=ftell(fp);
            //分配缓存
            char *http_body = (char*)malloc(sizeof(char)*file_size);
            //全部读入
            rewind(fp);
            fread(http_body,sizeof(char),file_size,fp);
            http_body[file_size] = '\0';
            //关闭文件
            fclose(fp);

            //生成HTTP请求头
            sprintf(buf_http_header,
                "HTTP/1.1 %d""\n"
                "Server: tiny-c-httpd""\n"
                "Content-Length: %lu""\n"
                "Connection: close""\n"
                "\n",
                http_status_code,strlen(http_body)
            );

            //发送数据给客户端
            /*
            函数原型：int send(int sockfd,const void* msg,int len,int flags)
            参数含义：
                sockfd：套接字描述符
                msg：指向要发送数据的指针
                len：数据长度
                flags：一般为0
            返回值：
                成功：发送的字节数
                出错：-1
            */
            //发送HTTP头部
            if((sendbytes = send(client_fd,buf_http_header,strlen(buf_http_header),0)) == -1) {
                perror("send");
                close(client_fd);
                break;
            }
            printf("发送HTTP头部: %d/%lu\n",sendbytes,strlen(buf_http_header));
            //发送网页内容
            if((sendbytes = send(client_fd,http_body,strlen(http_body),0)) == -1) {
                perror("send");
                close(client_fd);
                break;
            }
            printf("发送网页内容: %d/%lu\n",sendbytes,strlen(http_body));

            //发送完毕，关闭连接
            close(client_fd);
            printf("发送完毕，关闭连接。\n");
            break;

        }
    }

    close(sockfd);

}
