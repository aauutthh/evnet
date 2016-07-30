/**
* @file netevent.h
* @brief a network handle util
* @author Li Weidan
* @version 1.0
* @date 2016-06-21
*/

/*
* ev_net is a network handle util , it's drived by epoll event , all the elements 
* in ev_net is prefix of  "ev_"
*/

#ifndef __EV_NET_H__

#ifdef __cplusplus
extern "C" {
#endif

enum EV_TUNNELTYPE{
EV_TUNNELTYPE_UNSET = 0,
EV_TUNNELTYPE_LISTEN = 1,  // indicate that need to accept a new client
EV_TUNNELTYPE_SERVER ,     // indicate that a connected server has response arrive
EV_TUNNELTYPE_CLIENT ,     // indicate that a connected client has request arrive
};

// this struct is use in epoll_event->data
typedef struct epoll_event_data_s ev_data_t;


typedef int ev_handle(const ev_data_t* ed );
typedef void* ev_trigger(void* data, int datasize);

struct epoll_event_data_s{
    int fd;
    int epfd;
    int tunneltype;
    char* ip;
    unsigned short port;
    ev_handle *callback;
    void *callbackdata;
    int datasize;
} ;

int ev_init();
int ev_destroy();
int ev_listen_ipv4(const char* ip, const unsigned short port , ev_handle *f , 
                    void* callbackdata , int datasize );
int ev_connect_ipv4(const char* ip, const unsigned short port , ev_handle *f , 
                    void* callbackdata , int datasize );
int ev_accept(int fd , ev_handle *f , void* callbackdata , int datasize);
void ev_startloop();
//int ev_loop();
void ev_endloop();
#define ev_stoploop ev_endloop
int ev_ed_read(ev_data_t* ed , char* buf , int size);
int ev_ed_write(ev_data_t* ed ,char* buf , int size);
int ev_fd_read(int fd , char* buf , int size);
int ev_fd_write(int fd,char* buf , int size);
int ev_ed_close(const ev_data_t* ed );
int ev_fd_close(int fd);
void ev_set_timeout(int milisecond );
ev_data_t* ev_get_evdata(int fd);


/**
* @brief just recieve msg and response string '{status:"success"}' , 
*        when recieve "quit",will close the tunnel
*
* @param ed
*
* @return >=0:handle success  <0: handle error , will close the tunnel(fd ,ed , epfd ...)
*/
int ev_defaut_handle(const ev_data_t* ed  );

/**
* @brief set a  timetrigger handler , it will trigger per loop
* @param milisecond
* @param idx
* @param h
* @param cliendata
* @param datasize
*/
int ev_set_trigger(ev_trigger* h, void* cliendata , int datasize );
#define ev_clear_trigger() ev_set_trigger_per(NULL , NULL , 0)

#ifdef __cplusplus
}
#endif
#endif // __EV_NET_H__
