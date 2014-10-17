#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
int test_fun()
{
    char buf[1024];
    write(STDOUT_FILENO, buf, sizeof buf);
    return 0;
}

int main()
{
    test_fun();
    return 0;
}
