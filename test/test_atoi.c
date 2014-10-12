#include <stdio.h>
#include <stdlib.h>
int main()
{
    printf("%s : %d\n", "100", atoi("100"));
    printf("%s : %d\n", "-100", atoi("-100"));
    printf("%s : %d\n", "abc", atoi("abc"));
}

