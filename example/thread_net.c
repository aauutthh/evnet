/**
* @file thread_net.c
* @brief 
* @author Li Weidan
* @version 1.0
* @date 2016-07-30
* 
* 
* 
*  this example listen 2 port
*    4432 is the nomal port 
*    4433 is the control port , when sending quit , will endup the server
*  this example also connect to 4434. you can run 'nc -l -p 4434', the program will send the message to 'nc'
* 
* 
*/

#include "evm_net.h"
#include "assertion.h"
#include <stdio.h>
#include "signal_handler.h"
#include <signal.h>

#define BUFFER_SIZE 4096

int req_handle(const evm_data_t* ed);
int sendfd;
int ctlfd;

static void* timehandle(void* clientdata , int size)
{
    printf("timehandle\n");
}

static int loop = 1;
int on_signal_int(int signo)
{
   loop = 0; 
   printf("recieve sig int\n");
   evm_stop();
}



int regist_signal()
{
    register_signal(SIGINT, on_signal_int);
}

int main()
{
    regist_signal();
    evm_init(4);
    evm_listen_ipv4( NULL , 4432  , req_handle,(void*)1 , 0);
    ctlfd = evm_listen_ipv4( NULL , 4433  , req_handle,(void*)2 , 0);
    sendfd = evm_connect_ipv4( NULL , 4434 , req_handle,(void*)3 , 0);
    evm_set_timetrigger_per( 4000 ,1, timehandle ,  NULL , 0);
    //ev_startloop();
    evm_run();
    evm_join();
    evm_destroy();
}



int req_handle(const evm_data_t* ed)
{
    int cpu = sched_getcpu();
    int fd = ed->fd;
    int sfd = ed->sfd;
    RETURN_FALSE_UNLESS( fd > 0 );
    printf("from ip[%s] port[%d] callbackdata %p\n",ed->ip , ed->port , ed->callbackdata);
    static int ix;
    char  buf[BUFFER_SIZE];  
    int bufsize=BUFFER_SIZE;
    int recvsize = 0  ;
    recvsize = evm_fd_read(fd, buf, bufsize  );
    RETURN_FALSE_UNLESS( recvsize >= 0 );
    buf[recvsize] = 0;
    if( recvsize > 0 )
    {
        printf("in cpu %d . recv: %s",cpu  , buf);
        evm_fd_write(fd ,"recv sucess\n",12  );  
        evm_fd_write(sendfd ,buf,recvsize );  
    }
    if( strncmp(buf,"quit",4) == 0 )
    {
        evm_fd_close(fd);
        if( sfd == ctlfd && 1 == (unsigned int)(long)ed->callbackdata)
        {
            evm_stop();
        }
    }
    return 0 ;
}


