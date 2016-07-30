/*
 *
 *
 *
 */

#ifndef __SIGNAL_HANDLER_H__
#define __SIGNAL_HANDLER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void sigfunc(int);
typedef int signal_handler(int signo);

int register_signal(int signo, signal_handler* func);


int signal_retrieve();


#ifdef __cplusplus
}
#endif


#endif
