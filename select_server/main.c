#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>


#define ERRLOG(errmsg)                                       \
    do                                                       \
    {                                                        \
        printf("%s--%s(%d):", __FILE__, __func__, __LINE__); \
        perror(errmsg);                                      \
        exit(-1);                                            \
    } while (0)

//创建套接字-填充服务器网络信息结构体-绑定-监听
int socket_bind_listen(const char *argv[]);
void itoa(int i, char *string);

int main(int argc, const char *argv[])
{
    //检测命令行参数个数
    if (3 != argc)
    {
        printf("Usage : %s <IP> <PORT>\n", argv[0]);
        exit(-1);
    }
    //创建套接字-填充服务器网络信息结构体-绑定-监听
    int sockfd = socket_bind_listen(argv);

    //创建文件描述符表
    fd_set readfds;//母本
    FD_ZERO(&readfds);//清空
    //每次给select使用，因为select会擦除掉一部分
    fd_set readfds_temp; //用来备份原集合的
    FD_ZERO(&readfds_temp);//清空
    //记录表中最大的文件描述符
    int max_fd = 0;
    //将要监视的文件描述符放到表中
    FD_SET(sockfd, &readfds);
    max_fd = (max_fd > sockfd ? max_fd : sockfd);//记录最大文件描述符

    printf("listen_fd:%d\n",sockfd);

    int ret,ret1= 0;
    int accept_fd;
    char buff[128] = {0};
    int i;

    while (1)
    {
        // select 每次返回后 会将没有就绪的文件描述符擦除，所以
        //每次调用都需要重置这个集合
        readfds_temp = readfds;
//        struct timeval sttime;
//        sttime.tv_sec=20;
//        sttime.tv_usec=0;
        ret= select(max_fd + 1, &readfds_temp, NULL, NULL, NULL);
        if (ret == -1)
            ERRLOG("select error");
        //如果超时，提示timeout，不退出。现在为NULL不考虑超时
        if(ret==0){
            printf("Select timeout!\n");
            continue;
        }
        //说明有文件描述符准备就绪
        // && ret > 0 是为了提高效率：
        //select返回几个就绪的，就只处理几个就绪的就行了
        //对于其他没有就绪的文件描述符 无需判断处理
        for (i = 0; i < max_fd + 1&& ret > 0; i++)
        {
            if (FD_ISSET(i, &readfds_temp))//存在
            {//说明i就绪了
                if (i == sockfd)
                {
                    //阻塞等待客户端连接--一旦有客户端连接就会解除阻塞
                    struct sockaddr_in client;
                    socklen_t len= sizeof(client);
                    accept_fd = accept(sockfd, (struct sockaddr*)&client,&len);//NULL, NULL);
                    if (-1 == accept_fd)
                        ERRLOG("accept error");

                    printf("客户端 [%d] 连接了\n", accept_fd);
                    //将acceptfd也加入到要监视的集合中
                    FD_SET(accept_fd, &readfds);
                    //记录最大文件描述符
                    max_fd = (max_fd > accept_fd ? max_fd : accept_fd);
                }
                else
                {//说明有客户端发来消息了
                    accept_fd = i;
                    //接收客户端发来的数据
                    if (0 > (ret1 = recv(accept_fd, buff,sizeof(buff), 0)))
                    {
                        perror("recv error");
                        break;
                    }
                    else if (0 == ret1)//客户端CTRL+C
                    {
                        printf("客户端 [%d] 断开连接\n",accept_fd);
                        //将该客户端在集合中删除
                        FD_CLR(accept_fd, &readfds);
                        //关闭该客户端的套接字
                        close(i);
                        continue;//结束本层本次循环
                    }
                    else
                    {
                        if (0 == strcmp(buff, "quit"))
                        {
                            printf("客户端 [%d] 退出了\n",accept_fd);
                            //将该客户端在集合中删除
                            FD_CLR(accept_fd, &readfds);
                            //关闭该客户端的套节字
                            close(i);
                            continue;//结束本层本次循环
                        }

                        printf("客户端 [%d] 发来数据:[%s]\n", i, buff);
                        char len_buff[8];
                        itoa(strlen(buff),len_buff);
//                        printf("%d=%s字节\n", strlen(buff),len_buff);
                        //组装回复给客户端的应答
                        strcat(buff, "---from_serv:");
                        strcat(buff,len_buff);
                        //回复应答
                        if (0 > (ret1= send(accept_fd, buff,sizeof(buff), 0)))
                        {
                            perror("send error");
                            break;
                        }
                    }
                }
                ret--;
            }
        }
    }
    //关闭监听套接字  一般不关闭
    close(sockfd);
    return 0;
}

//创建套接字-填充服务器网络信息结构体-绑定-监听
int socket_bind_listen(const char *argv[])
{
    // 1.创建套接字      //IPV4   //TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
        ERRLOG("socket error");


    // 2.填充服务器网络信息结构体
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));//清空
    server_addr.sin_family = AF_INET;// IPV4
    //端口号  填 8888 9999 6789 ...都可以
    // atoi字符串转换成整型数
    //htons将无符号2字节整型  主机-->网络
    server_addr.sin_port = htons(atoi(argv[2]));
    // ip地址 要么是当前Ubuntu主机的IP地址 或者
    //如果本地测试的化  使用  127.0.0.1 也可以
    //inet_addr字符串转换成32位的网络字节序二进制值
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    //结构体长度
    socklen_t server_addr_len = sizeof(server_addr);

    // 3.将套接字和网络信息结构体绑定
    if (-1 == bind(sockfd, (struct sockaddr *)&server_addr, server_addr_len))
        ERRLOG("bind error");


    //将套接字设置成被动监听状态
    if (-1 == listen(sockfd, 10))
        ERRLOG("listen error");

    return sockfd;
}
void itoa(int lenstr, char *string)//3位的整型转字符串函数
{
    char aa[8]={0};
    int sum=lenstr;
    char *cp=string;
    char zm[11]="0123456789";
    int i=0;
    while(sum>0)
    {
        aa[i++]=zm[sum%10];
        sum/=10;
    }

    for(int j=i-1;j>=0;j--)
    {
        *cp++=aa[j];
    }
    *cp='\0';
}
