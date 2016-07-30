/*
 *
 *
 *
 */

#define _POSIX_SOURCE

#include <stdio.h>
#include <signal.h>
#include "signal_handler.h"


int g_exit = 0;
signal_handler* sig_funs[32]={
    0,0,0,0,  0,0,0,0,  0,0,0,0, 0,0,0,0,
    0,0,0,0,  0,0,0,0,  0,0,0,0, 0,0,0,0
};
int sig_happen[32]={
    0,0,0,0,  0,0,0,0,  0,0,0,0, 0,0,0,0,
    0,0,0,0,  0,0,0,0,  0,0,0,0, 0,0,0,0
};

sigfunc* old_sig_funs[32]={
    0,0,0,0,  0,0,0,0,  0,0,0,0, 0,0,0,0,
    0,0,0,0,  0,0,0,0,  0,0,0,0, 0,0,0,0
};

void sig_set(int signo)
{
    sig_happen[signo & 0x1F] = 1;
}


int register_signal(int signo, signal_handler* func)
{
    struct sigaction act, oact;
    if ( signo > 32 )
    {
        return -1;
    }
    if ( sig_funs[signo] != NULL )
    {
        sig_funs[signo] = func;
    }
    else
    {
        act.sa_handler = sig_set;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        if (sigaction(signo, &act, &oact) < 0)
            return -1;
        sig_funs[signo] = func;
        old_sig_funs[signo] = oact.sa_handler;
    }

    return 0;
}

int signal_retrieve()
{
    int retval = 0;
    int i = 1;
    for(; i < 32 ; i++ )
    {
        if( sig_happen[i] == 1 )
        {
            sig_happen[i] = 0;
            retval = sig_funs[i](i);
            if( i == SIGSEGV )
            {
                signal(i, SIG_DFL);
                raise(i);
            }
        }
    }

    return retval;
}
