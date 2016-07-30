#include "ev_net.h"
#include "assertion.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <memory.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include "signal_handler.h"

#define MSGTYPE msgtype
#define IPLEN 20
#define BUFFER_SIZE 4096

static int g_loop = 1; 
static int g_epfd = 1; 
static int g_timeout = 300; 

#define MAXFDNUM 4096
#define EPOLLQUEUESIZE 200
static struct epoll_event evs[EPOLLQUEUESIZE];
//static int g_fds[MAXFDNUM];
static ev_data_t* g_peds[MAXFDNUM];

static int g_len_listen_queue = 400; // 接受的排队客户端数
static ev_trigger* g_trigger  ;
static void *g_trigger_data ;
static int g_trigger_datasize ;

/*
* sock address util
*
*/

static struct sockaddr_in* ipv4_to_addr(const char* ip, const unsigned short port, 
        struct sockaddr_in* addr)
{

    bzero( addr, sizeof (struct sockaddr_in));  

    addr->sin_family  =  AF_INET;  
    addr->sin_port  =  htons(port);  
    if(NULL != ip && ip[0] != 0)
    {
        inet_pton(AF_INET, ip, &(addr->sin_addr));
    }
    else
    {
        addr->sin_addr.s_addr  =  htons(INADDR_ANY);  
    }
    return addr;
}

static char* ipv4_from_addr(struct sockaddr_in* addr , char* ip, unsigned short* port)
{
    inet_ntop(AF_INET, &(addr->sin_addr),ip, 30);   
    *port  = ntohs(addr->sin_port); 
    return ip;
}



/**
* file control utils
*
*
*/

/**
* @brief  setting file descriptor
*
* @param fd
* @param opt
* @param old_opts
* @param cleanflag  0:set 1:clean
*
* @return 
*/
static int fcntl_opt(int fd , int opt , int* old_opts ,int cleanflag )
{
    int newopts;
    *old_opts = fcntl(fd, F_GETFL);
    ASSERT( *old_opts > 0 , OR_RETURN , "fcntl F_GETFL failed %d\n",fd );

    if ( cleanflag == 1 ) // clean the fields
    {
        newopts = *old_opts & (!opt); 
    }
    else
    {
        newopts = *old_opts | opt;
    }

    int ret = fcntl(fd, F_SETFL, newopts) ;
    ASSERT( ret >= 0 , OR_RETURN , "fcntl F_SETFL failed, fd = %d\n", fd );

    return 0;
}

int set_nonblock(int fd)
{
    int old_opts;
    return fcntl_opt( fd , O_NONBLOCK , &old_opts , 0);
}



ev_data_t* create_ev_data(const char* ip, const unsigned short port , void* callbackdata , int datasize)
{
    int len = 2 ;
    if( NULL != ip)
    {
        len = strlen(ip) + 1;
    }
    int size = sizeof(ev_data_t) ;
    ev_data_t* p = (ev_data_t*)malloc( size + len + datasize + 1);
    bzero( p, size + len + datasize + 1 );  
    p->port = port ;
    p->ip = (char*)p + size;
    if( NULL != ip)
    {
        strncpy( p->ip  , ip , len);
    }
    else
    {
        p->ip[0] = '0' ;
        p->ip[1] = 0 ;
    }

    p->callbackdata = (char*)p + size + len;
    p->datasize = datasize ;
    memcpy( p->callbackdata , callbackdata , datasize );

    return p;
}

void free_ev_data(ev_data_t* p)
{
    free(p);
}


static int listen_ipv4(const char* ip, const unsigned short port)
{
    struct sockaddr_in srvaddr;  
    int srvfd ;
    int reuse = 1;
    int ret = 0 ;

    srvfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT(srvfd>0, OR_RETURN ,"socket fail" , 0);

    ipv4_to_addr(ip , port , &srvaddr);
    setsockopt(srvfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse, sizeof(int));
    ret = bind(srvfd,( struct  sockaddr * ) & srvaddr, sizeof (srvaddr)) ;
    ASSERT( ret == 0 , OR_RETURN , "bind return %d\n" , ret );

    ret = listen(srvfd,g_len_listen_queue) ;
    ASSERT( ret == 0 , OR_RETURN , "listen return %d\n" , ret );
    return srvfd; 
}

static int connect_ipv4(const char* ip, const unsigned short port)
{
    struct sockaddr  conaddr;  
    int confd ;
    int reuse = 1;

    confd = socket(AF_INET, SOCK_STREAM, 0);
    ipv4_to_addr(ip , port , (struct sockaddr_in*)&conaddr);
    setsockopt(confd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse, sizeof(int));

    int ret =  connect(confd, &conaddr, sizeof(conaddr) ) ;
    ASSERT( ret == 0 , OR_RETURN , "connect return %d\n" , ret ); 

    return confd; 
}
int ev_listen_ipv4(const char* ip, const unsigned short port ,ev_handle *f , void* callbackdata , int datasize)
{
    int epollfd = g_epfd;
    int fd = listen_ipv4(ip, port);
    ASSERT( fd > 0 , OR_RETURN , "listen_ipv4 return fd %d\n",fd);

    struct epoll_event ev ;
    ev_data_t * ed = create_ev_data(ip , port , callbackdata , datasize);
    ed->fd = fd;
    ed->epfd = epollfd;
    ed->tunneltype = EV_TUNNELTYPE_LISTEN;
    ed->callback = f;

    ev.events = EPOLLIN | EPOLLERR |EPOLLRDHUP | EPOLLET;
    ev.data.ptr = ed;

    epoll_ctl(epollfd, EPOLL_CTL_ADD,  fd, &ev);
    set_nonblock(fd);
    g_peds[fd] = ed ;
    return fd;
}
int ev_connect_ipv4( const char* ip, const unsigned short port , ev_handle *f , void* callbackdata , int datasize)
{
    int epollfd = g_epfd;
    int fd = connect_ipv4(ip, port);
    ASSERT( fd > 0 , OR_RETURN , "connect_ipv4 return fd %d\n",fd);

    //printf("connect fd %d\n",fd);
    struct epoll_event ev ;
    ev_data_t *ed = create_ev_data(ip , port , callbackdata , datasize);
    ed->fd = fd;
    ed->epfd = epollfd;
    ed->tunneltype = EV_TUNNELTYPE_SERVER;
    ed->callback = f;

    ev.events = EPOLLIN | EPOLLERR |EPOLLRDHUP | EPOLLET ;
    ev.data.ptr = ed;

    epoll_ctl(epollfd, EPOLL_CTL_ADD,  fd, &ev);
    set_nonblock(fd);
    g_peds[fd] = ed;
    return fd;
}

static int _epoll_accept(int fd , ev_handle* f , void* callbackdata , int datasize)
{
    int epfd = g_epfd;
    char  buf[BUFFER_SIZE];  
    int bufsize=BUFFER_SIZE;
    long  timestamp;  
    struct  sockaddr_in cliaddr; 
    socklen_t length  =   sizeof (cliaddr);  
    int clifd  =  accept(fd,( struct  sockaddr * ) & cliaddr, & length);  
    ASSERT( clifd > 0 , OR_RETURN , "call accept fail" , 0);

    char ip[30];
    unsigned short port;
    ipv4_from_addr(&cliaddr, ip , &port );

    printf( "accept from client,IP:%s,Port:%d , fd:%d\n" ,  ip,port , clifd);  


    struct epoll_event ev ;
    ev_data_t *ed = create_ev_data(ip , port , callbackdata , datasize);
    ed->fd = clifd;
    ed->epfd = epfd;
    ed->tunneltype = EV_TUNNELTYPE_CLIENT;
    ed->callback = f;

    ev.events = EPOLLIN | EPOLLERR |EPOLLRDHUP | EPOLLET ;
    ev.data.ptr = ed;

    epoll_ctl(epfd, EPOLL_CTL_ADD,  clifd, &ev);
    set_nonblock(clifd);
    g_peds[clifd] = ed;

#ifndef __LIB_SO__ 
    strcpy(buf,"hello");  
    timestamp  =  time(NULL);  
    strcat(buf, " timestamp in server: " );  
    strcat(buf,ctime( & timestamp));  
    send(clifd,buf,strlen(buf), 0 );  
#endif

    return clifd;
}


static int _ev_loop()
{
    int epfd = g_epfd;
    while  ( g_loop )  
    { 
        signal_retrieve();

        if ( g_trigger !=  NULL )
        {
            g_trigger( g_trigger_data , g_trigger_datasize); 

        }

        int n_evs = epoll_wait(epfd, evs, 1000, g_timeout);
        int i = 0 ;
        
        PRINTF_WHEN( n_evs>0 ,  "%d events \n",n_evs) ;

        for( ; i < n_evs ; i++ )
        {
            ev_data_t* ed = (ev_data_t*)evs[i].data.ptr;
            int fd = ed->fd;
            if ( evs[i].events & EPOLLERR )
            {
                printf("fd %d ,epoll error\n",fd);
                ev_ed_close(ed);
            }
            else if ( (evs[i].events & EPOLLRDHUP) && 1)
            {
                printf("fd %d ,EPOLLRDHUP ,peer shutdown\n",fd);
                ev_ed_close(ed);
            }
            else if ( evs[i].events & ( EPOLLIN | EPOLLET) )
            {
                if ( ed->tunneltype == EV_TUNNELTYPE_LISTEN )
                {
                    printf("ev:0x%x fd:%d\n",evs[i].events , fd );
                    int ret = _epoll_accept(fd , ed->callback , ed->callbackdata ,ed->datasize );
                    // ASSERT_TRUE( ret >= 0 , ev_ed_close(ed) ) ;
                }
                else
                {
                    printf("ev:0x%x fd:%d\n",evs[i].events , fd );
                    //call ev_handle(fd);
                    int ret =  ed -> callback( ed ) ;
                    ASSERT_TRUE( ret >= 0 , ev_ed_close(ed) ) ;
                }
            }
            else if ( evs[i].events & EPOLLOUT )
            {
                //
            }
        }
    }
}

void ev_startloop()
{
    g_loop = 1;
    _ev_loop();
    
}

int ev_init()
{
    g_epfd = epoll_create(10);
    ASSERT( g_epfd >= 0 , OR_RETURN , "epoll_create fail",0 );
    memset(evs , 0 , sizeof(evs) );
    memset(g_peds , 0 , sizeof(g_peds) );
    return 0 ;
}

int ev_destroy()
{
    int i = 0 ;
    for( i = 0 ; i< MAXFDNUM ; i++)
    {
        if(g_peds[i] != NULL )
        {
            ev_ed_close(g_peds[i]);
        }
    }
    close(g_epfd);
}

void ev_endloop()
{
    g_loop = 0;
}

void ev_set_timeout(int mili)
{
    g_timeout = mili;
}

int ev_ed_close(const ev_data_t* ed )
{
    RETURN_FALSE_UNLESS( NULL != ed );

    int fd = ed->fd;
    int epfd = ed->epfd;
    free_ev_data((ev_data_t*)ed);
    epoll_ctl(epfd, EPOLL_CTL_DEL ,  fd ,NULL);
    if( g_peds[fd] != 0 )
    {
        close(fd);
        g_peds[fd] = NULL ;
    }
    return 0;
}
int ev_fd_close(int fd)
{
    return ev_ed_close(g_peds[fd] );
}

int ev_ed_read(ev_data_t* ed , char* buf , int size)
{
    int fd = ed->fd;
    ASSERT(fd > 0 , OR_RETURN , "fd %d\n" , fd);
    return  recv(fd, buf, size , MSG_DONTWAIT);
}


int ev_fd_read(int fd , char* buf , int size)
{
    return  recv(fd, buf, size , MSG_DONTWAIT);
}

int ev_ed_write(ev_data_t* ed ,char* buf , int size)
{
    int fd = ed->fd;
    ASSERT(fd > 0 , OR_RETURN , "fd %d\n" , fd);
    return send(fd , buf, size , 0 );  
}


int ev_fd_write(int fd ,char* buf , int size)
{
    return send(fd , buf, size , 0 );  
}

ev_data_t* ev_get_evdata(int fd)
{
    return g_peds[fd] ;
}

int ev_defaut_handle(const ev_data_t* ed)
{
    int fd = ed->fd;
    printf("from ip[%s] port[%d]\n",ed->ip , ed->port );
    static int ix;
    char  buf[BUFFER_SIZE];  
    int bufsize=BUFFER_SIZE;
    int recvsize ;
    int sendsize ;
    recvsize = ev_fd_read(fd, buf, bufsize );
    buf[recvsize] = 0 ;
    printf("recv:%s",buf);
    int quitflag = 0 ;
    if( strncmp(buf,"quit",4) == 0 )
    {
        quitflag = 1;
    }
    if( recvsize > 0 )
    {
        sprintf(buf , "{status:\"sucess\",recieve:\"%d\"}\n" , recvsize);
        sendsize = ev_fd_write(fd ,buf ,strlen(buf) );  
        printf("sendsize %d\n",sendsize);
    }
    if( quitflag )
    {
        ev_ed_close(ed);
    }
    return 0;
}

int ev_set_trigger(ev_trigger* h, void* cliendata , int datasize )
{
    g_trigger = h ;
    g_trigger_data = cliendata ;
    g_trigger_datasize = datasize ;
}

