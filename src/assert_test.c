#include <stdio.h>
#include <err.h>

static char buf[4096];

#define ASSERT( condiction , optype , fmt , args...) \
    if ( optype(condiction) ) \
    { \
        sprintf(buf , "%s in %s[%d] ASSERT fail:"#fmt ,__FUNCTION__ , __FILE__ ,  __LINE__ , ##args); \
        optype##_PRT(buf); \
        optype##_CMD; \
    } \

#define OR_RETURN(x) (x)==0
#define OR_RETURN_CMD return -1
#define OR_RETURN_PRT printf

int main()
{
    ASSERT( 1>2 , OR_RETURN , "hello %s", "world");
return 0;
}
