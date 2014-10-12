#include <stdio.h>
#include <pthread.h>
#define my_entry struct {\
    int a;\
    int b;\
}

struct a {
    my_entry;
};

int main()
{
    struct a x;
    x.a = 1;
    x.b = 2;
    printf("%d-%d\n", x.a, x.b);
    printf("%u\n", sizeof(pthread_t));
}
