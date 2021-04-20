#define _GNU_SOURCE 1  //使用POLLRDHUP事件时，需要定义_GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>

#define BUFFER_SIZE 64

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    struct sockaddr_in server_address;
    bzero( &server_address, sizeof( server_address ) );
    server_address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &server_address.sin_addr );
    server_address.sin_port = htons( port );

    int sockfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sockfd >= 0 );
    if ( connect( sockfd, ( struct sockaddr* )&server_address, sizeof( server_address ) ) < 0 )
    {
        printf( "connection failed\n" );
        close( sockfd );
        return 1;
    }

    pollfd fds[2];
    //注册文件描述符0（标准输入）和文件描述符sockfd上的可读事件
    fds[0].fd = 0;
    fds[0].events = POLLIN;// 数据可读
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP; //POLLRDHUP：TCP连接被对方关闭，或者对方关闭了写操作（可以用来判断时有效数据还是关闭连接的请求）
    fds[1].revents = 0;
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe( pipefd );
    //pipe函数用于创建一个管道，以实现进程间通信，pipe函数的参数是一个包含两个int整数的数组指针。
    //该函数成功时返回0，并将一对打开的文件描述符值填入其参数指向的数组
    assert( ret != -1 );
    
    while( 1 )
    {
        ret = poll( fds, 2, -1 );//nfds表示被监听时间集合fds的大小，这里为2
        //第三个参数为超时值：单位为毫秒，当timeout为-1时，poll调用将永远阻塞，知道某个时间发生，当为0时，调用将立即返回。
        if( ret < 0 )
        {
            printf( "poll failure\n" );
            break;
        }

        if( fds[1].revents & POLLRDHUP )
        {
            printf( "server close the connection\n" );
            break;
        }
        else if( fds[1].revents & POLLIN )//监听socket文件描述符
        {
            memset( read_buf, '\0', BUFFER_SIZE );//memset：将某一块内存中的内容全部设为指定的值
            recv( fds[1].fd, read_buf, BUFFER_SIZE-1, 0 ); //将监听到的socket可读数据写入缓冲区
            printf( "%s\n", read_buf );
        }

        if( fds[0].revents & POLLIN ) //监听标准输入文件描述符
        {//利用splice函数将用户输入内容直接定向到网络连接上以发送之（零拷贝）
            //将标准输入数据写入管道
            ret = splice( 0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );
            ret = splice( pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE );//从管道的读出端移动到sockfd
        }
    }
    
    close( sockfd );
    return 0;
}
