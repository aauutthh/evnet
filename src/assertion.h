/**
* @file assertion.h
* @brief 
* @author Li Weidan
* @version 1.0
* @date 2016-06-21
*/

#ifndef __ASSERTION_H__
#define __ASSERTION_H__
#ifndef ASSERTION_BUFSIZE 
#define ASSERTION_BUFSIZE 4096
#endif
static char __ASSERTION_BUF__[ASSERTION_BUFSIZE] ;



/**
* @brief  ASSERT*  make an assertion , when your assertion is correct , it will do nothing
*        otherwise , it will do the command you give, and print an error msg to stderr
*
* @param p
* @param x
* @param msg
* @param (x
*
* @return 
*/

#define ASSERT_TRUE( condiction , STATEMENTS ) \
    if ( (condiction) == 0  ) \
    { \
        sprintf(__ASSERTION_BUF__, "%s in %s[%d]" ,__FUNCTION__ , __FILE__ ,  __LINE__  ); \
        ASSERT_TRUE_PRINT_CMD(__ASSERTION_BUF__); \
        STATEMENTS ; \
    } \

#define ASSERT_TRUE_PRINT_CMD printf

#define ASSERT_AUX2( p , x , msg, ret )  \
        if( (x) == 0 )  \
        { \
            p("%s at %s in %s[%d]\n" , __FUNCTION__ ,__FILE__ ,  __LINE__ ); \
            ret ; \
        } 

#if 0
#define ASSERT_EXIT( x , fmt ) ASSERT_AUX( warn , x , ftm, exit(-1) )  
#define ASSERT_RETURN( x , fmt ) ASSERT_AUX( warn , x , ftm, return -1 )  
#define ASSERT_SET( x , fmt ) ASSERT_AUX( warn , x , ftm, ret = -1 )  
#endif

#define ASSERT_EXIT( x , fmt , ...) \
    if( (x) == 0 )  \
    { \
        sprintf( buf , "%s at %s in %s[%d]\n"fmt ,__FUNCTION__ ,  __FILE__ ,  __LINE__ , __VA_ARGS__ ); \
        warn(buf ); \
        exit(-1); \
    }

#define ASSERT_RETURN( x , fmt , args...) \
    if( (x) == 0 )  \
    { \
        sprintf( __ASSERTION_BUF__ , "%s at %s in %s[%d]\n"#fmt ,__FUNCTION__ ,  __FILE__ ,  __LINE__ , ##args); \
        warn(__ASSERTION_BUF__); \
        return -1; \
    }

#define ASSERT_SET( x , fmt , ...) \
    if( (x) == 0 )  \
    { \
        sprintf(__ASSERTION_BUF__, "%s at %s in %s[%d]\n"#fmt ,__FUNCTION__ , __FILE__ ,  __LINE__ , __VA_ARGS__ ); \
        warn(__ASSERTION_BUF__); \
        ret = -1; \
    }

#define ASSERT_TRUE_PRINT( x , fmt , ... )   \
        if( (x) )  \
        { \
            printf( fmt, __VA_ARGS__  ); \
        } 



#define ASSERT( condiction , optype , fmt , args...) \
    if ( optype(condiction) ) \
    { \
        sprintf(__ASSERTION_BUF__, "%s in %s[%d] ASSERT fail:"fmt ,__FUNCTION__ , __FILE__ ,  __LINE__ , ##args); \
        optype##_PRT(__ASSERTION_BUF__); \
        optype##_CMD; \
    } \

#ifndef OR_RETURN
#define OR_RETURN(x) (x)==0
#endif
#ifndef OR_RETURN_CMD
#define OR_RETURN_CMD return -1
#endif
#ifndef OR_RETURN_PRT
#define OR_RETURN_PRT printf
#endif

#ifndef OR_EXIT
#define OR_EXIT(x) (x)==0
#endif
#ifndef OR_EXIT_CMD
#define OR_EXIT_CMD exit(-1)
#endif
#ifndef OR_EXIT_PRT
#define OR_EXIT_PRT err
#endif

#ifndef OR_SET_RET
#define OR_SET_RET(x) (x)==0
#endif
#ifndef OR_SET_RET_CMD
#define OR_SET_RET_CMD ret = -1
#endif
#ifndef OR_SET_RET_PRT
#define OR_SET_RET_PRT printf
#endif

#ifndef THEN_PRINT
#define THEN_PRINT(x) (x) != 0 
#endif
#ifndef THEN_PRINT_CMD
#define THEN_PRINT_CMD 0!=0
#endif
#ifndef THEN_PRINT_PRT
#define THEN_PRINT_PRT printf
#endif

#define ASSERT_DO(condiction , optype , jobdo) \
    if ( optype(condiction) ) \
    { \
        jobdo ; \
    } 

#define OR_DO(x) (x) == 0
#define THEN_DO(x) (x) != 0


// code is comment.
#define DO_WHEN(condiction , jobdo) \
    if ( (condiction) != 0  ) \
    { \
        jobdo; \
    }

#define DO_UNLESS(condiction , jobdo) \
    if ( (condiction) == 0  ) \
    { \
        jobdo; \
    }

#define RETURN_FALSE_UNLESS(condiction ) DO_UNLESS(condiction , return -1 )
#define RETURN_FALSE_WHEN(condiction ) DO_WHEN(condiction , return -1 )


#define PRINTF_WHEN(condiction , fmt , args...) \
    if ( (condiction) != 0  ) \
    { \
        printf(fmt , ##args);\
    }

#define PRINTF_UNLESS(condiction , fmt , args...) \
    if ( (condiction) == 0  ) \
    { \
        printf(fmt , ##args);\
    }


#endif // __ASSERTION_H__
