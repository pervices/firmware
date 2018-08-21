#include <stdio.h>

#define LIST_OF_VARIABLES \
    X(a) \
    X(b) \
    X(c) \
    X(d)

#define X(ch) \
    void fun_##ch##(void) {                      \
        printf("%d %c\n", #ch[0] - 'a', #ch[0]); \
    }
    LIST_OF_VARIABLES
#undef X


int main()
{
#define X(ch) \
    fun_##ch();
    LIST_OF_VARIABLES
#undef X
}
