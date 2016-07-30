/**
* @file evm_net.h
* @brief ev_net like network interface , providing multithreads suport
* @author Li Weidan
* @version 1.0
* @date 2016-06-23
*/

#ifndef __EVM_NET_H__

#define EVM_DEFAULT_THREAD_NUM 3
#define EVM_MAX_THREAD_NUM 1000
#define EVM_MAX_QUEUE_SIZE 100
#define EVM_MAX_FD 4096
#define EVM_TIME_TRIGGER_NUMS 10

typedef struct evm_data_s evm_data_t;
typedef int evm_handle(const evm_data_t* evm );
typedef void* evm_timehandler(void* clientdata , int size);

typedef struct {
    int tid;
} evm_tinfo_t;

struct evm_data_s{
    int fd;
    int sfd;  // accept from sfd
    char* ip;
    unsigned short port;
    evm_handle *callback;
    void *callbackdata;
    int datasize;
} ;


int evm_init(int thread_num);
int evm_set_thread_num(int thread_num);
void evm_destroy();
int evm_run();

/**
* @brief signal all the thread to stop running
*
* @return 
*/
int evm_stop();

/**
* @brief wait for thread join 
*
* @return 
*/
int evm_join();


int evm_listen_ipv4( char* ip , unsigned short port , evm_handle* f , 
        void* callbackdata , int datasize );
int evm_connect_ipv4( char* ip , unsigned short port , evm_handle* f , 
        void* callbackdata , int datasize );

int evm_ed_read(evm_data_t* ed , char* buf , int size);
int evm_ed_write(evm_data_t* ed ,char* buf , int size);
int evm_fd_read(int fd , char* buf , int size);
int evm_fd_write(int fd,char* buf , int size);
int evm_ed_close(const evm_data_t* ed );
int evm_fd_close(int fd );
void evm_set_epoll_timeout(int milisecond );

/**
* @brief set a  timetrigger per milisecond  , the trigger will callback  function h
*        , the idx is limit to [0,10) , when set h to NULL , will remove the trigger
*
* @param milisecond
* @param idx
* @param h
* @param cliendata
* @param datasize
*/
int evm_set_timetrigger_per(int milisecond ,int idx ,  
             evm_timehandler* h, void* cliendata , int datasize );
#define evm_clear_timetrigger(idx) evm_set_timetrigger_per(0,idx , NULL , NULL , 0)

#endif // __EVM_NET_H__

