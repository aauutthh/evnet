#include "ev_net.h"
#include "evm_net.h"
#include "assertion.h"
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUFFER_SIZE 4096


// threads control
static pthread_mutex_t g_t_mutex[EVM_MAX_THREAD_NUM];
static pthread_cond_t g_t_sig[EVM_MAX_THREAD_NUM];
static pthread_attr_t g_attr;

// threads pointers
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

static unsigned int _hash(unsigned int key);
static void* _evm_thread(void *tinfo);
static int _evm_default_handle(const ev_data_t* ed );
static int _at_handle_error(int err , const evm_data_t* ed);

static int g_trigger_encount[EVM_TIME_TRIGGER_NUMS];
static int g_trigger[EVM_TIME_TRIGGER_NUMS];
static evm_timehandler* g_trigger_handler[EVM_TIME_TRIGGER_NUMS];
static void* g_trigger_data[EVM_TIME_TRIGGER_NUMS];
static pthread_mutex_t g_trigger_mutex;
static pthread_cond_t g_trigger_sig;
static pthread_t* g_trigger_thread;

static int jobqueue_push( int tid , int fd)
{
    int* q = g_jobq[tid];
    int ix = g_jobix[tid];
    RETURN_FALSE_UNLESS( ix < EVM_MAX_THREAD_NUM  ); 

    q[ix] = fd;
    g_jobix[tid]++;
    return 0;
}
 
static int jobqueue_pop( int tid )
{
    int* q = g_jobq[tid];
    int ix = g_jobix[tid];
    RETURN_FALSE_UNLESS ( ix > 0 ) ;

    g_jobix[tid]--;
    int ret = q[ix-1];
    q[ix-1] = 0 ;
    return ret;
}

static int jobqueue_search( int tid , int fd)
{
    int* q = g_jobq[tid];
    int ix = g_jobix[tid];
    RETURN_FALSE_UNLESS ( ix > 0 ) ;

    ix--;
    for( ; ix >= 0 ; ix-- )
    {
        if ( fd == q[ix] )
            { return ix; }
    }
    return -1;
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


int evm_run()
{
    g_run = 1;
    ev_startloop();
}
int evm_stop()
{
    g_run = 0;
    ev_endloop();  
    int i ;
    for(i=0 ; i < g_thread_num ; i ++ )
    { 
        pthread_mutex_t* mutex = &g_t_mutex[i];
        pthread_cond_t* sig= &g_t_sig[i];
        pthread_mutex_lock(mutex);
        pthread_cond_signal(sig);
        pthread_mutex_unlock(mutex);
    }
        pthread_mutex_lock(&g_trigger_mutex);
        pthread_cond_signal(&g_trigger_sig);
        pthread_mutex_unlock(&g_trigger_mutex);
}
int evm_join()
{
    g_run = 0 ;
    int i ;
    for(i=0 ; i < g_thread_num ; i ++ )
    { 
     pthread_join(*g_threads[i], NULL);
    }
     pthread_join(*g_trigger_thread, NULL);
}

void evm_destroy()
{
    ev_destroy();
    int i;
    for ( i = 0 ; i < g_thread_num ; i++) 
    {
        DO_WHEN( g_threads[i]!=0 , free(g_threads[i]) );
        pthread_mutex_destroy(&g_t_mutex[i]);
        pthread_cond_destroy(&g_t_sig[i]);
    }
        pthread_mutex_destroy(&g_trigger_mutex);
        pthread_cond_destroy(&g_trigger_sig);

    pthread_attr_destroy(&g_attr);

    for( i = 0 ; i < EVM_TIME_TRIGGER_NUMS ; i++ )
    {
        DO_WHEN( NULL != g_trigger_data[i] , free ( g_trigger_data[i] ) ) ;
        g_trigger[i] = 0;
        g_trigger_handler[i] = NULL;
        g_trigger_data[i] = NULL;
    }
    //pthread_exit(NULL);
}

static unsigned long getmilisecond()
{
    struct timeval t; 
    gettimeofday(&t , NULL);
    return t.tv_sec * 1000 + t.tv_usec / 1000 ;
}

struct _d{
    int size;
    char data;
};

static void* _evm_default_trigger(void* data, int datasize)
{
    int i ;
    static unsigned long now = 0;
    static unsigned long last[EVM_TIME_TRIGGER_NUMS];
    if ( now == 0 )
    {
        memset(last , 0 , sizeof(last) );
    }
    now = getmilisecond();
    for( i = 0 ; i < EVM_TIME_TRIGGER_NUMS ; i ++ ) 
    {
        if ( g_trigger[i] != 0 && g_trigger_handler[i] != NULL)
        {
            if (now - last[i]  > g_trigger[i] ) 
            {
                //printf("trigger %d : now %ld , last %ld \n" , i , now , last[i]);
                // todo: to a separate thread
                g_trigger_encount[i] = 1 ;
                last[i] = now;
            }
        }
    }

}


static void* _evm_trigger_thread(void* data)
{
    while(g_run)
    {
        // wait for sig
        pthread_mutex_lock(&g_trigger_mutex);
        pthread_cond_wait(&g_trigger_sig, &g_trigger_mutex);


        int i ;
        for( i = 0 ; i < EVM_TIME_TRIGGER_NUMS ; i ++ ) 
        {
            if ( g_trigger_encount[i] != 0 && g_trigger_handler[i] != NULL)
            {
                int size = ((struct _d*)g_trigger_data[i]) -> size;
                void* d = &( ((struct _d*)g_trigger_data[i]) -> data);
                g_trigger_handler[i](d , size);
                g_trigger_encount[i] = 0 ;
            }
        }
        pthread_mutex_unlock(&g_trigger_mutex);
    }
    pthread_exit(NULL);
}

int evm_init(int thread_num)
{
    int i,ret;
    ev_set_trigger(_evm_default_trigger, NULL , 0);

    g_run = 1;
    ev_init();
    memset(g_datas , 0 , sizeof(g_datas) );
    memset(g_jobq  , 0 , sizeof(g_jobq) );
    memset(g_jobix , 0 , sizeof(g_jobix) );
    memset(g_threads , 0 , sizeof(g_threads) );
    memset(g_trigger , 0 , sizeof(g_trigger ) ) ;
    memset(g_trigger_handler , 0 , sizeof(g_trigger_handler ) ) ;

    for ( i = 0 ; i < g_thread_num ; i++) 
    {
        g_threads[i] = (pthread_t*) malloc(sizeof(pthread_t ));
        pthread_cond_init (&g_t_sig[i], NULL);
        pthread_mutex_init(&g_t_mutex[i], NULL);
    }
        g_trigger_thread = (pthread_t*)malloc(sizeof(pthread_t ));
        pthread_cond_init (&g_trigger_sig, NULL);
        pthread_mutex_init(&g_trigger_mutex, NULL);

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
            return -1;
        }
    }
        ret = pthread_create(g_trigger_thread, &g_attr, _evm_trigger_thread, (void *)999);
    

    return(0);
}



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

        int ret ;
        DO_WHEN( func != NULL , ret = func( ed ) ) ;
        DO_WHEN( ret < 0 , _at_handle_error(ret , ed) );


    }
    free(tinfo);
    pthread_exit(NULL);
}

int evm_listen_ipv4( char* ip , unsigned short port , evm_handle* f , 
        void* callbackdata , int datasize )
{
    evm_data_t d ;
    memset(&d , 0 , sizeof(d));
    d.callback = f;
    d.callbackdata = callbackdata;
    d.datasize = datasize;
    int fd = ev_listen_ipv4( ip , port, _evm_default_handle , (void*)&d , sizeof(evm_data_t));
    RETURN_FALSE_WHEN( fd < 0 );
    ev_data_t* ed = ev_get_evdata(fd);
    evm_data_t* evm = (evm_data_t*)ed->callbackdata ;
    evm->sfd = fd;
    evm->ip=ed->ip;
    evm->port=ed->port;
   return fd; 
}

int evm_connect_ipv4( char* ip , unsigned short port , evm_handle* f , 
        void* callbackdata , int datasize )
{
    evm_data_t d ;
    memset(&d , 0 , sizeof(d));
    d.callback = f;
    d.callbackdata = callbackdata;
    d.datasize = datasize;
    int fd = ev_connect_ipv4(ip, port , _evm_default_handle , (void*)&d , sizeof(evm_data_t));
    RETURN_FALSE_WHEN( fd < 0 );
    ev_data_t* ed = ev_get_evdata(fd);
    evm_data_t* evm = (evm_data_t*)ed->callbackdata ;
    evm->sfd = fd;
    evm->ip=ed->ip;
    evm->port=ed->port;
   return fd; 
}


/**
* @brief _evm_*  handle innert with  ev_* datatype define in ev_net.h ,
*
* @param ed
*
* @return 
*/
static int _evm_default_handle(const ev_data_t* ed )
{
    // 通知thread启动
    int fd = ed->fd;
    evm_data_t* d = (evm_data_t*)ed->callbackdata;
    if ( g_datas[fd] != d)
    {
        g_datas[fd] = d;
    }
        d->fd = fd;
        d->ip = ed->ip;
        d->port = ed->port;

    int t_ix = _hash(fd) % (g_thread_num - 1) ; // thread index
    int ret = pthread_mutex_lock(&g_t_mutex[t_ix]);
    if ( ret != 0 )
      { return 0 ;}
    if ( jobqueue_search( t_ix , fd) == -1 )
    {
        jobqueue_push( t_ix , fd);
    }
    // waitup threads[t_ix]
    pthread_cond_signal(&g_t_sig[t_ix]);
    pthread_mutex_unlock(&g_t_mutex[t_ix]);
    return 0;
}


/**
* @brief wrap ev_* from ev_net.h , for entirely interface for user
*
* @param ed
* @param buf
* @param size
*
* @return 
*/
int evm_ed_read(evm_data_t* ed , char* buf , int size)
{
    return ev_fd_read(ed->fd , buf , size);
}

int evm_ed_write(evm_data_t* ed ,char* buf , int size)
{
    return ev_fd_write(ed->fd , buf , size);
}
int evm_fd_read(int fd , char* buf , int size)
{
    return ev_fd_read(fd , buf , size);
}
int evm_fd_write(int fd,char* buf , int size)
{
    return ev_fd_write(fd , buf , size);
}
int evm_ed_close(const evm_data_t* ed )
{
    return ev_fd_close(ed->fd );
}
int evm_fd_close(int fd )
{
    return ev_fd_close(fd );
}
void evm_set_epoll_timeout(int milisecond )
{
    ev_set_timeout(milisecond);
}
int evm_set_timetrigger_per(int milisecond ,int idx ,  
        evm_timehandler* h, void* cliendata , int datasize )
{
    RETURN_FALSE_UNLESS ( idx < EVM_TIME_TRIGGER_NUMS );
    if ( NULL == h ) 
    {
        g_trigger[idx] = 0 ;
        g_trigger_handler[idx] = NULL ;
        DO_WHEN( g_trigger_data[idx] != NULL , free ( g_trigger_data[idx] )  );
        g_trigger_data[idx] = NULL ;
    }
    else
    {
        g_trigger[idx] = milisecond ;
        g_trigger_handler[idx] = h;
        DO_WHEN( g_trigger_data[idx] != NULL , free ( g_trigger_data[idx] )  );
        g_trigger_data[idx] =  malloc (sizeof(datasize) + datasize ) ;
        DO_WHEN( NULL == g_trigger_data[idx] , evm_set_timetrigger_per(0 , idx , NULL , NULL , 0 ) );
    }
    return 0;
}

static int _at_handle_error(int err , const evm_data_t* ed)
{
    if ( err < 0 )
    {
        evm_ed_close(ed);
    }
}


