#include <stdio.h>

#define CHR(ch) (#ch[0])
#define INT(ch) (CHR(ch) - 'a')

#define LIST_OF_VARIABLES \
    X(a) \
    X(b) \
    X(c) \
    X(d) \
    X(e) \
    X(f) \
    X(g) \
    X(h) \
    X(i) \
    X(j) \
    X(k) \
    X(l) \
    X(m) \
    X(n) \
    X(o) \
    X(p) \

#define X(ch) \
    void fun_##ch(void) {                    \
        printf("%2d %c\n", INT(ch), CHR(ch)); \
    }
    LIST_OF_VARIABLES
#undef X

int main()
{
#define X(ch) fun_##ch();
    LIST_OF_VARIABLES
#undef X
}
