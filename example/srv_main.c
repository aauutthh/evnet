#include "ev_net.h"
#include "assertion.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

int req_handle(const ev_data_t* ed);
int sendfd;

int main()
{
    ev_init();
    ev_listen_ipv4( NULL , 4432  , ev_defaut_handle);
    ev_listen_ipv4( NULL , 4433  , ev_defaut_handle);
    sendfd = ev_connect_ipv4( NULL , 4434 , req_handle);
    ev_startloop();
    ev_destroy();
}

int req_handle(const ev_data_t* ed)
{
    int fd = ed->fd;
    return 0;
    printf("from ip[%s] port[%d]\n",ed->ip , ed->port );
    static int ix;
    char  buf[BUFFER_SIZE];  
    int bufsize=BUFFER_SIZE;
    int recvsize ;
    recvsize = ev_fd_read(fd, buf, bufsize  );
    buf[recvsize] = 0;
    if( recvsize > 0 )
    {
        printf("recv: %s",buf);
        ev_fd_write(fd ,"recv sucess\n",12  );  
        ev_fd_write(sendfd ,buf,recvsize );  
    }
    if( strncmp(buf,"quit",4) == 0 )
    {
        close(fd);
        if( fd = sendfd )
        {
            ev_fd_write(sendfd ,"i'm closing",11);  
            ev_endloop();
        }
    }
    return 0 ;
}


