#include "ev_net.h"
#include "assertion.h"
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 4096
#define EVM_DEFAULT_THREAD_NUM 3
#define EVM_MAX_THREAD_NUM 1000
#define EVM_MAX_QUEUE_SIZE 100
#define EVM_MAX_FD 4096

typedef struct evm_data_s evm_data_t;
typedef int evm_handle(const evm_data_t* evm );
int evm_listen_ipv4( char* ip , unsigned short port , evm_handle* f , 
        void* callbackdata , int datasize );
static void* _evm_thread(void *tinfo);
static unsigned int _hash(unsigned int key);

int _evm_default_handle(const ev_data_t* ed );


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


//static int ev_multi
static pthread_mutex_t g_t_mutex[EVM_MAX_THREAD_NUM];
static pthread_cond_t g_t_sig[EVM_MAX_THREAD_NUM];
static pthread_attr_t g_attr;

// threads points
static pthread_t* g_threads[EVM_MAX_THREAD_NUM];
static int g_thread_num = EVM_DEFAULT_THREAD_NUM ;

// g_datas[fd] is *->1  to thread .  it is meaning that
//        one thread process many fd and g_datas[fd] . 
//        one fd is in on thread
static evm_data_t* g_datas[EVM_MAX_FD];

// jobqueues
// jobix[i] is the ith thread's jobs count , g_t_mutex[i] is  needed to be locked when access
// jobqueue[i] is the ith thread's jobqueue , g_t_mutex[i] is  needed to be locked when access
// jobqueue[i][ jobix[i] ] is the ith thread's newest job ,it's a fd num
static int g_jobq[EVM_MAX_THREAD_NUM][EVM_MAX_QUEUE_SIZE+1];
static int g_jobix[EVM_MAX_THREAD_NUM];

static int g_run;
int _evm_default_handle(const ev_data_t* ed );
int jobqueue_push( int tid , int fd)
{
    int* q = g_jobq[tid];
    int ix = g_jobix[tid];
    if ( ix >= EVM_MAX_THREAD_NUM  ) //
    {
        return -1;
    } 
    q[ix] = fd;
    g_jobix[tid]++;
    return 0;
}
 
int jobqueue_pop( int tid )
{
    int* q = g_jobq[tid];
    int ix = g_jobix[tid];
    if ( ix <= 0 ) //
    {
        return -1;
    } 
    g_jobix[tid]--;
    return q[ix-1];
}
int jobqueue_search( int tid , int fd)
{
    int* q = g_jobq[tid];
    int ix = g_jobix[tid];
    if ( ix <= 0 ) //
    {
        return -1;
    } 
    ix--;
    for( ; ix >= 0 ; ix-- )
    {
        if ( fd == q[ix] )
            { return ix; }
    }
    return -1;
}

int req_handle(const evm_data_t* ed);
int sendfd;

int evm_init(int thread_num);
void evm_destroy();
int evm_run();
int evm_join();

int main()
{
    evm_init(4);
    evm_listen_ipv4( NULL , 4432  , req_handle,(void*)0 , 0);
    //evm_listen_ipv4( NULL , 4433  , evm_defaut_handle,(void*)1);
    //sendfd = evm_connect_ipv4( NULL , 4434 , req_handle,(void)*2);
    //ev_startloop();
    evm_run();
    evm_join();
    evm_destroy();
}

void evm_destroy()
{
    ev_destroy();
}

int evm_init(int thread_num)
{
    int i,ret;

    g_run = 1;
   ev_init();
   memset(g_datas , 0 , sizeof(g_datas) );
   memset(g_jobq  , 0 , sizeof(g_jobq) );
   memset(g_jobix , 0 , sizeof(g_jobix) );
   memset(g_threads , 0 , sizeof(g_threads) );

    for ( i = 0 ; i < g_thread_num ; i++) 
    {
        g_threads[i] = (pthread_t*) malloc(sizeof(pthread_t ));
        pthread_cond_init (&g_t_sig[i], NULL);
        pthread_mutex_init(&g_t_mutex[i], NULL);
    }

   /* For portability, explicitly create threads in a joinable state */
   pthread_attr_init(&g_attr);
   pthread_attr_setdetachstate(&g_attr, PTHREAD_CREATE_JOINABLE);
    //初始化线程池
    for ( i = 0 ; i < g_thread_num ; i++) 
    {
        evm_tinfo_t* t = (evm_tinfo_t*)malloc( sizeof(evm_tinfo_t) );
        t -> tid = i ;
        ret = pthread_create(g_threads[i], &g_attr, _evm_thread, (void *)t);
        if (ret != 0)
        {
            printf("pthread create failed!\n");
            return(-1);
        }
    }

    // ret = pthread_create(&threadId, 0, check_connect_timeout, (void *)0);

    return(0);
}


#if 0
int req_handle_thread(ev_data_t* ed )
{
    int fd = ed->fd;
    g_handlemap[fd](*ed , ed->callbackdata)
}
#endif

static void* _evm_thread(void *tinfo)
{
    int tid = ((evm_tinfo_t*)tinfo)->tid;
    pthread_mutex_t* mutex = &g_t_mutex[tid];
    pthread_cond_t* sig= &g_t_sig[tid];
    while(g_run)
    {
        // wait for sig
        pthread_mutex_lock(mutex);
        pthread_cond_wait(sig, mutex);

        int fd = jobqueue_pop( tid  );
        if ( fd <= 0 ) 
        {
            pthread_mutex_unlock(mutex);
            continue;
        }
        evm_data_t* ed = g_datas[fd];
        evm_handle* func = ed->callback ;

        pthread_mutex_unlock(mutex);

        int ret = func( ed );

    }
    free(tinfo);
}
int evm_run()
{
    g_run = 1;
    ev_startloop();
}
int evm_stop()
{
    g_run = 0;
}
int evm_join()
{
    g_run = 0 ;
    for(;;)
    {
       // join
    }
}

int evm_listen_ipv4( char* ip , unsigned short port , evm_handle* f , 
        void* callbackdata , int datasize )
{
    evm_data_t d ;
    d.callback = f;
    d.callbackdata = callbackdata;
    d.datasize = datasize;
    int fd = ev_listen_ipv4( NULL , 4432  , _evm_default_handle , (void*)&d , sizeof(evm_data_t));
   return fd; 
}


/**
* @brief _evm_*  handle innert with  ev_* datatype define in ev_net.h ,
*
* @param ed
*
* @return 
*/
int _evm_default_handle(const ev_data_t* ed )
{
    // 通知thread启动
    int fd = ed->fd;
    evm_data_t* d = (evm_data_t*)ed->callbackdata;
    if ( g_datas[fd] != d)
    {
        g_datas[fd] = d;
        d->fd = fd;
        d->ip = ed->ip;
        d->port = ed->port;
    }

    int t_ix = _hash(fd) % (g_thread_num - 1) ; // thread index
    if ( jobqueue_search( t_ix , fd) != -1 )
    {
        return 0;
    }
    int ret = pthread_mutex_lock(&g_t_mutex[t_ix]);
    if ( ret != 0 )
      { return 0 ;}
    // todo :insert threads queue 
    jobqueue_push( t_ix , fd);
    // waitup threads[t_ix]
    pthread_cond_signal(&g_t_sig[t_ix]);
    pthread_mutex_unlock(&g_t_mutex[t_ix]);
    return 0;
}

// user interface use evm_* datatype
int req_handle(const evm_data_t* ed)
{
    int fd = ed->fd;
    printf("from ip[%s] port[%d]\n",ed->ip , ed->port );
    static int ix;
    char  buf[BUFFER_SIZE];  
    int bufsize=BUFFER_SIZE;
    int recvsize ;
    // todo:  donot use ev_*
    recvsize = ev_fd_read(fd, buf, bufsize  );
    buf[recvsize] = 0;
    if( recvsize > 0 )
    {
        printf("recv: %s",buf);
    // todo:  donot use ev_*
        ev_fd_write(fd ,"recv sucess\n",12  );  
        ev_fd_write(sendfd ,buf,recvsize );  
    }
    if( strncmp(buf,"quit",4) == 0 )
    {
    // todo:  use evm_close(ed)
        close(fd);
#if 0
        if( fd = sendfd )
        {
    // todo:  donot use ev_*
            ev_fd_write(sendfd ,"i'm closing",11);  
            ev_endloop();
        }
#endif
    }
    return 0 ;
}


static unsigned int _hash(unsigned int key)
{
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}
